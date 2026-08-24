/*

ここではUDPの受信，スレッドプールの管理を行う．
UDPを受信したのちそのパケットをスレッドプールに渡して次のパケットに備える．
スレッドプール内のスレッド数分だけ受信用のバッファをもっておき，1スレッドにつき1つ割り当てる．
UDP受信用のバッファには未使用スレッドのバッファを割り当てて，受信後に該当のバッファを持つスレッドを起こす．

スレッドプール内の処理ではRTPヘッダの解析，J2Kコードストリームの解析を行う．

解析結果をハードウェアデコードに渡す部分は未定
・マルチスレッドの場合，パケットは順番どおりでも，解析速度がばらついて解析終了時の順番は前後するかも
・上記が起こらない保証があれば不要？
・スレッドプール内からの場合，パケットの前後が変化しないようにする必要がある
・ハードウェアデコーダに渡すためのスレッドを立ち上げる．スレッド間通信に注意，パケットの整列を担当させる．<- パケットの到着順序をチェックするならこれかも

実装予定のCPUはコア4つ -> 4つ以上はあんまり意味ない，せいぜい2つ？

*/

#include "UDP.hpp"
#include "RTP_header.hpp"
#include "leaky_bucket_buf.hpp"

#include "codestream.hpp"
#include "decoding_unit.hpp"

#include "opt_macro.hpp"

#include "argument.hpp"

#include <vector>
#include <string_view>
#include <charconv>
#include <cassert>
#include <array>
#include <thread>

#include <pthread.h>

#include <csignal>

constexpr size_t MAX_PACKET_SIZE = 1384;
sig_atomic_t sig_flag            = 0;
void sig_handler(int sig_num [[maybe_unused]]) {
    sig_flag = 1;
}

int main(int argc, char** argv) {
    std::string_view addr = "127.0.0.1";
    uint16_t port         = 50001;
    size_t out_flame      = 60;
    cpu_set_t affinity;
    CPU_ZERO(&affinity);
    cpu_set_t affinity_analysis;
    CPU_ZERO(&affinity_analysis);
    constexpr size_t MAX_BUFFER_LENGTH = 1360 * 10;
    size_t buffer_length               = MAX_BUFFER_LENGTH;
    bool is_enter                      = false;
    enum class OutF : uint8_t {
        FPS,
        MS
    };
    OutF output_format       = OutF::FPS;
    using clock_t            = std::chrono::high_resolution_clock;
    size_t prealloc_precinct = 0;

#ifdef RTP_CLOCK_CHECK
    size_t clock_check_size = 120;
#endif

    {
        using namespace tklib;
        static constexpr argument_list opts(
            optspec_t{'a', "address", true, "IPv4 address default: 127.0.0.1"},
            optspec_t{'p', "port", true, "Port default: 50001"},
            optspec_t{'f', "frame", true, "The interval between frames to display default: 60"},
            optspec_t{'c', "receive_affinity", true, "CPU affinity of the receive thread"},
            optspec_t{'C', "analysis_affinity", true, "CPU affinity of the analysis thread"},
            optspec_t{'b', "BufferLength", true, "Receive buffer length, default: 13600, max: 13600"},
            optspec_t{0, "Precinct", true, "Number of precinct's to be allocated in advance"},
            optspec_t{0, "Enter", false, "analysis thread continue at enter"},
            optspec_t{0, "OutputFormat", true, "this option is determines the output format for the frame rate. value: fps or ms, default: fps"},
#ifdef RTP_CLOCK_CHECK
            optspec_t{0, "ClockCheckSize", true, "Number of frames to verify the clock rate. default: 120"},
#endif
            optspec_t{'h', "help", false, "Show this"}
        );
        argument_t args(argc, argv, opts);
        while (args.can_parse()) {
            switch (args.getopt()) {
                case opts('a'):
                    addr = args.get_str();
                    break;
                case opts('p'):
                    if (auto tmp = args.get_value<uint16_t>(); tmp) port = tmp.value();
                    break;
                case opts('f'):
                    if (auto tmp = args.get_value<size_t>(); tmp) out_flame = tmp.value();
                    break;
                case opts('c'): {
                    size_t cpu_bit_mask = 0;
                    if (auto tmp = args.get_value<size_t>(); tmp) {
                        cpu_bit_mask = tmp.value();
                        size_t i     = 0;
                        while (cpu_bit_mask != 0) {
                            if (cpu_bit_mask & 0x1)
                                CPU_SET(i, &affinity);
                            cpu_bit_mask >>= 1;
                            ++i;
                        }
                    }
                } break;
                case opts('C'): {
                    size_t cpu_bit_mask = 0;
                    if (auto tmp = args.get_value<size_t>(); tmp) {
                        cpu_bit_mask = tmp.value();
                        size_t i     = 0;
                        while (cpu_bit_mask != 0) {
                            if (cpu_bit_mask & 0x1)
                                CPU_SET(i, &affinity_analysis);
                            cpu_bit_mask >>= 1;
                            ++i;
                        }
                    }
                } break;
                case opts('b'): {
                    if (auto tmp = args.get_value<size_t>(); tmp) {
                        buffer_length = tmp.value();
                        if (buffer_length > MAX_BUFFER_LENGTH) {
                            fprintf(stderr, "-b: the maximum value is %ld\n", MAX_BUFFER_LENGTH);
                            exit(1);
                        }
                    }
                } break;
                case opts("Precinct"):
                    prealloc_precinct = args.get_value<size_t>().value_or(0);
                    break;
                case opts("Enter"):
                    is_enter = true;
                    break;
                case opts("OutputFormat"): {
                    const auto tmp = args.get_str();
                    if (tmp == "fps") {
                        output_format = OutF::FPS;
                    } else if (tmp == "ms") {
                        output_format = OutF::MS;
                    } else {
                        fprintf(stderr, "unknown parameter: %s\n", tmp.data());
                        exit(1);
                    }
                } break;
#ifdef RTP_CLOCK_CHECK
                case opts("ClockCheckSize"):
                    if (auto tmp = args.get_value<size_t>(); tmp) clock_check_size = tmp.value();
                    break;
#endif
                case opts('h'):
                    printf("Options:\n%s\n", TKLIB_ARG_DESCRIPTION(opts, dft_style_fmt));
                    exit(0);
                case opts(opt_err::ambiguous): {
                    std::array<std::string_view, 3> amb;
                    const auto re = opts.get_ambiguous(args.get_last_parse(), amb.begin(), amb.end());
                    std::cerr << "'--" << args.get_last_parse() << "' is ambiguous as ";
                    for (auto it = amb.begin(); it != re; ++it) std::cerr << *it << " ";
                    std::cerr << std::endl;
                    exit(1);
                }
                case opts(opt_err::no_argument): {
                    std::cerr << "'" << args.get_last_parse() << "' requires an argument" << std::endl;
                }
                default:
                    std::cerr << "'" << args.get_last_parse() << "' is unknown argument" << std::endl;
                    exit(1);
            }
        }
    }

    UDPReceiver udp(addr.data(), port);
    static leaky_bucket_buf::link_list packet_buffer[MAX_BUFFER_LENGTH]{};
    leaky_bucket_buf buffer(&udp, packet_buffer, buffer_length);
    RTPReceiver rtp_recv(&buffer);

    clock_t::time_point avg_frame;
    size_t analysis_frame          = 0;
    size_t loss_frame              = 0;
    size_t interpolate_frame       = 0;
    uint32_t frame_lost_precinct   = 0;
    size_t RTP_error_count         = 0;
    size_t J2K_error_count         = 0;
    size_t error_counts[3]         = {};
    long double sum_avg            = 0;
    size_t sum_lost_packet         = 0;
    double analysis_operating_time = 0;
    double receive_operating_time  = 0;

    std::atomic_bool analysis_stoper = false;

    std::mutex img_clock_locker;
    std::condition_variable img_clock_cond;
    std::atomic_uint32_t img_inc = 0;
    clock_t::time_point img_clock;

    auto to_duration = [](const auto t) constexpr { return std::chrono::duration_cast<clock_t::duration>(std::chrono::duration<double>{t * (1.0 / J2KPayloadHeader_trait::media_clock_Hz)}); };

#ifdef RTP_CLOCK_CHECK
    std::vector<clock_t::time_point> debug_clock_check{clock_check_size};
    auto debug_clock_it           = debug_clock_check.begin();
    const auto debug_clock_it_end = debug_clock_check.end();
#endif

    struct sigaction sa{};
    sigemptyset(&sa.sa_mask);
    sa.sa_flags   = 0;
    sa.sa_handler = sig_handler;
    if (sigaction(SIGINT, &sa, nullptr) == -1) {
        perror("sigaction(SIGINT)");
        exit(1);
    }

    sigset_t new_sig_set;
    sigemptyset(&new_sig_set);
    sigaddset(&new_sig_set, SIGINT);
    if (sigprocmask(SIG_BLOCK, &new_sig_set, nullptr) == -1) {
        perror("sigprocmask");
        exit(1);
    }

    /***************************************************************************************************/
    /* analysis_thread                                                                                 */
    /***************************************************************************************************/

    std::thread analysis_thread([&] {
        clock_t::time_point analysis_start;
        clock_t::time_point analysis_finish;
        size_t table_index     = 0;
        uint32_t PID           = 0;
        uint32_t last_sequence = 0;

        j2k_Tile::resource_t prealloc;
        if (prealloc_precinct != 0) {
            prealloc.prev_allocate(prealloc_precinct, prealloc_precinct * 3);
        }
        if (unlikely(is_enter)) {
            while (!analysis_stoper);
        }
        printf("analysis thread ready...\n");
        analysis_start = clock_t::now();

#ifndef DISABLE_TABLE
        MainHeader main_header;
        j2k_Tile j2k_tile;
        j2k_tile.move_resource(std::move(prealloc));
        auto& j2k_packet_table = j2k_tile.acs_table();
        {
            int result = 0;
            clock_t::time_point t;
            while (true) {
                result = rtp_recv.load_main_packet();
                if (result == RTPReceiver::MAIN_HEADER) {
                    t = clock_t::now();
                    break;
                }
                if (result == RTPReceiver::FINISH) { return; }
            }
            img_clock = clock_t::now();
            avg_frame = img_clock;
            J2kBuf buf(&rtp_recv);
            main_header.read(buf);
            j2k_tile.init(main_header, buf);
            auto r = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - t);
            printf("main header read, seq: %d, time: %ldns\n", rtp_recv.get_last_sequence_number(), r.count());
        }
#endif
#ifdef DISABLE_TABLE
        img_clock = clock_t::now();
        avg_frame = img_clock;
#endif
        while (!sig_flag) {
#ifdef DISABLE_TABLE
            MainHeader main_header;
            j2k_Tile j2k_tile;
            auto& j2k_packet_table = j2k_tile.acs_table();
            {
                int32_t result = 0;
                while (true) {
                    result = rtp_recv.load_main_packet();
                    if (result == RTPReceiver::MAIN_HEADER) { break; }
                    if (result == RTPReceiver::FINISH) { return; }
                }
                avg_frame = clock_t::now();
                J2kBuf buf(&rtp_recv);
                main_header.read(buf);
                j2k_tile.init(main_header, buf);
                // printf("main header read, seq: %d\n", rtp_recv.get_last_sequence_number());
            }
            while (!sig_flag)
#endif
                try {
                    auto frame_update = [&]() {
                        if (frame_lost_precinct != 0) {
                            const auto lost_per = static_cast<double>(frame_lost_precinct) / j2k_tile.get_total_precinct() * 100;
                            fprintf(
                                stderr,
                                "    analysis_frame: %ld, lost_precinct: %d/%ld, %.6lf%%\n",
                                analysis_frame, frame_lost_precinct, j2k_tile.get_total_precinct(), lost_per
                            );
                            frame_lost_precinct = 0;
                        }
                        ++analysis_frame;

                        img_clock += to_duration(img_inc.load(std::memory_order_acquire));
                        // std::this_thread::sleep_until(img_clock);
                        const auto now                     = clock_t::now();
                        [[maybe_unused]] const auto jitter = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(now - img_clock).count();
#ifdef RTP_CLOCK_CHECK
                        if (debug_clock_it != debug_clock_it_end) *debug_clock_it = now;
                        ++debug_clock_it;
#endif
                        if (out_flame != 0 && analysis_frame % out_flame == 0) {
                            auto avg = std::chrono::duration_cast<std::chrono::microseconds>(now - avg_frame);
                            if (output_format == OutF::FPS) {
                                const auto avg_fps = 1 / ((static_cast<double>(avg.count()) / 1000) / out_flame) * 1000;
                                sum_avg += avg_fps;
                                // printf("analysis_frame: %ld, avg: %.6f fps\n", analysis_frame, avg_fps);
                                printf("analysis_frame: %ld, avg: %.6f fps, in buf: %ld\n", analysis_frame, avg_fps, buffer.get_num_data());
                                // printf("analysis_frame: %ld, avg: %.6f fps, in buf: %ld, sleep jitter: %lf ms\n", analysis_frame, avg_fps, buffer.get_num_data(), jitter);
                            } else if (output_format == OutF::MS) {
                                const auto avg_ms = (static_cast<double>(avg.count()) / out_flame) / 1000;
                                sum_avg += avg_ms;
                                // printf("analysis_frame: %ld, avg: %.6f ms\n", analysis_frame, avg_ms);
                                printf("analysis_frame: %ld, avg: %.6f ms, in buf: %ld\n", analysis_frame, avg_ms, buffer.get_num_data());
                                // printf("analysis_frame: %ld, avg: %.6f ms, in buf: %ld, sleep jitter: %lf ms\n", analysis_frame, avg_ms, buffer.get_num_data(), jitter);
                            }
                            avg_frame = now;
                        }
                    };

                    last_sequence          = rtp_recv.get_last_sequence_number();
                    const auto recv_result = rtp_recv.load_body_packet();
                    if (likely(recv_result == RTPReceiver::SUCCESS)) { // 正常受信
                        J2kBuf buf(&rtp_recv);
                        PID = rtp_recv.get_PID();
                        while (table_index < j2k_packet_table.size() && j2k_packet_table[table_index].get_PID() != PID) {
                            j2k_packet_table[table_index].read_packet(buf);
                            ++table_index;
                        }
                        if (unlikely(table_index == j2k_packet_table.size())) {
                            auto m = buf.get_byte(2);
                            assert(m == j2kmk::EOC);
                            rtp_recv.terminate();
                            table_index = 0;
                            frame_update();
#ifdef DISABLE_TABLE
                            break;
#endif
                        }
                    } else if (recv_result == RTPReceiver::MAIN_HEADER) { // メインパケット出現

                    } else if (recv_result == RTPReceiver::FAILURE) { // パケットロス
                        PID                  = rtp_recv.get_PID();
                        size_t loss_precinct = 0;
                        ++RTP_error_count;
                        while (true) {
                            if (j2k_packet_table[table_index].get_PID() == PID) break;
                            ++loss_precinct;
                            ++table_index;
                            if (table_index == j2k_packet_table.size()) {
                                table_index = 0;
                                frame_lost_precinct += loss_precinct;
                                frame_update();
                            }
                        }
                        frame_lost_precinct += loss_precinct;
                        if (unlikely(rtp_recv.get_lost_packet() > loss_precinct)) {
                            loss_frame += 1;
                            loss_precinct = table_index;
                        }
                        fprintf(
                            stderr,
                            "  RTP error analysis_frame: %ld, lost_packet: %d, discarded_packet: %d, lost_precinct: %ld, in buf: %ld\n",
                            analysis_frame, rtp_recv.get_lost_packet(), rtp_recv.get_last_sequence_number() - last_sequence, loss_precinct, buffer.get_num_data()
                        );
                        sum_lost_packet += rtp_recv.get_lost_packet();
                    } else {
#ifdef DISABLE_TABLE
                        goto long_break;
#endif
                        break;
                    }

                } catch (buffer_leak& e) {
                    // buffer.clear();
                    const auto in_buf = buffer.get_num_data();
                    bool is_terminate = false;
                    fputs(e.what(), stderr);
                    fflush(stderr);
                    const size_t dest_packet = buffer.dest(
                        [&](const uint8_t* const data) { return J2KPayloadHeader_trait::get_MH(data + RTPHeader_trait::length) ||
                                                                RTPHeader_trait::get_V(data) != 0b10; },
                        [&](const uint8_t* const data) { is_terminate = RTPHeader_trait::get_V(data) != 0b10; }
                    );
                    fprintf(stderr, ": buffer leak error analysis_frame: %ld, discarded packsts: %ld, in buf: %ld\n", analysis_frame, dest_packet, in_buf);
                    ++loss_frame;
                    ++J2K_error_count;
                    table_index = 0;
                    ++error_counts[e.type];
                    if (is_terminate) break;
                }
        }
#ifdef DISABLE_TABLE
    long_break:
#endif
        analysis_finish         = clock_t::now();
        analysis_operating_time = std::chrono::duration_cast<std::chrono::milliseconds>(analysis_finish - analysis_start).count() / 1000.0;
        printf("analysis finish\n");
    });

    /***************************************************************************************************/
    /* receive_thread                                                                                  */
    /***************************************************************************************************/

    std::thread receive_thread([&]() {
#ifdef GENERATE_RECEIVE_PROBABILITY
        size_t count_receive = 0;
        size_t count_again   = 0;
#endif
        sigset_t new_sig_set;
        sigemptyset(&new_sig_set);
        sigaddset(&new_sig_set, SIGINT);
        if (pthread_sigmask(SIG_UNBLOCK, &new_sig_set, nullptr) == -1) {
            perror("pthread_sigmask");
            exit(1);
        }

        clock_t::time_point receive_start;
        clock_t::time_point receive_finish;
        uint32_t pre_timestamp = 0;
        uint32_t pre_TPSTAMP   = 0;
        uint32_t pre_flow      = 0;

        printf("receive thread ready...\n");
        receive_start    = clock_t::now();
        bool is_img_init = false;
        while (!sig_flag) {
            const auto result = buffer.receive();
#ifdef GENERATE_RECEIVE_PROBABILITY
            if (result == leaky_bucket_buf::AGAIN) ++count_again;
            if (result == leaky_bucket_buf::RECEIVED) ++count_receive;
#endif
            if (result == leaky_bucket_buf::RECEIVED) {
                const auto* pkt = buffer.get_last_packet();
                if (J2KPayloadHeader_trait::get_MH(pkt->data + RTPHeader_trait::get_header_length())) { // EOCの有無を確認 メインヘッダで確認に変更
                    const auto tp = RTPHeader_trait::get_timestamp(pkt->data);
                    // if (!is_img_init && pre_timestamp != 0 && tp != 0) {
                    //     // std::unique_lock lk{img_clock_locker};
                    //     img_inc.store(tp - pre_timestamp, std::memory_order_release);
                    //     img_clock_cond.notify_one();
                    //     is_img_init = true;
                    // }
                    // img_clock_cond.notify_one();
                    pre_timestamp = tp;
                }
                const auto TPS = J2KPayloadHeader_trait::get_extended_sequence_number(pkt->data);
                if (TPS != pre_TPSTAMP + 1 && TPS && pre_TPSTAMP) {
                    const auto flow = udp.get_overflow_packet();
                    if (pre_flow != flow) {
                        fprintf(stderr, "lost packet: %d, overflow packet: %d\n", TPS - (pre_TPSTAMP + 1), flow - pre_flow);
                        pre_flow = flow;
                    } else {
                        fprintf(stderr, "Loss due to transmission path, total overflow pakcet: %d\n", flow);
                    }
                }
                pre_TPSTAMP = TPS;
            } else if (result == leaky_bucket_buf::AGAIN) {
            } else if (result == leaky_bucket_buf::SIGNAL) {
                img_clock_cond.notify_one();
                break;
            } else if (likely(result == leaky_bucket_buf::FINISH)) {
                img_clock_cond.notify_one();
                break;
            }
        }
        receive_finish         = clock_t::now();
        receive_operating_time = std::chrono::duration_cast<std::chrono::milliseconds>(receive_finish - receive_start).count() / 1000.0;
        if (sig_flag) putc('\n', stdout);
        printf("receive finish\n");
#ifdef GENERATE_RECEIVE_PROBABILITY
        printf("receive: %ld\n", count_receive);
        printf("again:   %ld\n", count_again);
        printf("receive probability: %lf%% \n", static_cast<double>(count_receive) / static_cast<double>(count_receive + count_again) * 100);
#endif
        {
            uint8_t tb = 0;
            buffer.push(&tb, 1);
        }
    });

    if (CPU_COUNT(&affinity) != 0) {
        if (auto result = pthread_setaffinity_np(receive_thread.native_handle(), sizeof(affinity), &affinity); result != 0) {
            fprintf(stderr, "pthread_setaffinity_up() error: %d\n", result);
            exit(1);
        }
    }
    if (CPU_COUNT(&affinity_analysis) != 0) {
        if (auto result = pthread_setaffinity_np(analysis_thread.native_handle(), sizeof(affinity_analysis), &affinity_analysis); result != 0) {
            fprintf(stderr, "pthread_setaffinity_up() error: %d\n", result);
            exit(1);
        }
    }

    if (unlikely(is_enter)) {
        printf("Press Enter to continue\n");
        getc(stdin);
        analysis_stoper = true;
    }

    analysis_thread.join();
    receive_thread.join();

    printf("=============================================\n");
    printf("args:");
    for (int i = 1; i < argc; ++i) { printf(" %s", argv[i]); }
    putc('\n', stdout);
    printf("analysis thread's operating time: %lfs\n", analysis_operating_time);
    printf("receive thread's operating time: %lfs\n", receive_operating_time);
    if (output_format == OutF::FPS) {
        printf("average fps: %Lffps\n", sum_avg / (analysis_frame / out_flame));
    } else {
        printf("average fps: %Lfms\n", sum_avg / (analysis_frame / out_flame));
    }
    printf("analysis frame: %ld\n", analysis_frame);
    printf("interpolate  frame: %ld\n", interpolate_frame);
    printf("lost frame: %ld\n", loss_frame);
    printf("lost packets: %ld/%ld\n", sum_lost_packet, analysis_frame * 1360);
    printf("packet loss rate: %lf%%\n", (sum_lost_packet / (analysis_frame * 1360.0)) * 100);
    printf("RTP packet error: %ld\n", RTP_error_count);
    printf("J2K packet error: %ld\n", J2K_error_count);
    for (int i = 0; i < 3; ++i) printf("leak_error[%d]: %ld\n", i, error_counts[i]);
    printf("in buf: %ld\n", buffer.get_num_data());

#ifdef RTP_CLOCK_CHECK
    {
        // printf("clock result, print each value? [y/n]");
        // char yn[16];
        // scanf("%c", yn);
        // bool is_print = (yn[0] == 'y' || yn[0] == 'Y');
        bool is_print = false;

        long double sum_check = 0;
        auto pre_check        = std::chrono::duration_cast<std::chrono::microseconds>(debug_clock_check.front().time_since_epoch()).count() / 1000.0;
        for (size_t i = 1; i < debug_clock_check.size(); ++i) {
            auto now = std::chrono::duration_cast<std::chrono::microseconds>(debug_clock_check[i].time_since_epoch()).count() / 1000.0;
            if (is_print) printf("%lfms\n", now - pre_check);
            sum_check += now - pre_check;
            pre_check = now;
        }
        printf("avg: %lfms\n", static_cast<double>(sum_check / (debug_clock_check.size() - 1)));
    }
#endif

    return 0;
}
