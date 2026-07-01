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
#include "decoding.hpp"
#include "const_value.hpp"

#include "fast_table.hpp"

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
void sig_handler(int sig_num) {
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
    size_t buffer_length = 1360 * 10;
    double rf_r          = 0.5;
    double rf_a          = 0.25;
    bool is_enter        = false;
    enum class OutF : uint8_t {
        FPS,
        MS
    };
    OutF output_format = OutF::FPS;

#ifdef RTP_CLOCK_CHECK
    size_t clock_check_size = 120;
#endif

    {
        using namespace tklib;
        static constexpr argument_list args_list(
            {{'a', "address", "IPv4 address default: 127.0.0.1"},
             {'p', "port", "Port default: 50001"},
             {'f', "frame", "The interval between frames to display default: 60"},
             {'c', "receive_affinity", "CPU affinity of the receive thread"},
             {'C', "analysis_affinity", "CPU affinity of the analysis thread"},
             {'b', "BufferLength", "Maximum number of packets to buffer, default: 13600"},
             {0, "ReceiveFrequency", "The multiplier for the receiving thread frequency (90kHz). Fromat: \"receive:again\",default: 0.5:0.25"},
             {0, "Enter", "analysis thread continue at enter"},
             {0, "OutputFormat", "this option is determines the output format for the frame rate. value: fps or ms, default: fps"},
#ifdef RTP_CLOCK_CHECK
             {0, "ClockCheckSize", "Number of frames to verify the clock rate. default: 120"},
#endif
             {'h', "help", "Show this"}}
        );
        static_assert(args_list.check());
        argument_t args(argc, argv, args_list);
        while (!args.empty()) {
            switch (args.get_opt()) {
                case args_list('a'):
                    addr = args.pop();
                    break;
                case args_list('p'): {
                    auto tmp = args.pop();
                    std::from_chars(tmp.begin(), tmp.end(), port);
                } break;
                case args_list('f'): {
                    auto tmp = args.pop();
                    std::from_chars(tmp.begin(), tmp.end(), out_flame);
                } break;
                case args_list('c'): {
                    size_t cpu_bit_mask = 0;
                    auto tmp            = args.pop();
                    if (std::from_chars(tmp.begin(), tmp.end(), cpu_bit_mask, 16).ptr == tmp.end()) {
                        size_t i = 0;
                        while (cpu_bit_mask != 0) {
                            if (cpu_bit_mask & 0x1)
                                CPU_SET(i, &affinity);
                            cpu_bit_mask >>= 1;
                            ++i;
                        }
                    }
                } break;
                case args_list('C'): {
                    size_t cpu_bit_mask = 0;
                    auto tmp            = args.pop();
                    if (std::from_chars(tmp.begin(), tmp.end(), cpu_bit_mask, 16).ptr == tmp.end()) {
                        size_t i = 0;
                        while (cpu_bit_mask != 0) {
                            if (cpu_bit_mask & 0x1)
                                CPU_SET(i, &affinity_analysis);
                            cpu_bit_mask >>= 1;
                            ++i;
                        }
                    }
                } break;
                case args_list('b'): {
                    auto tmp = args.pop();
                    std::from_chars(tmp.begin(), tmp.end(), buffer_length);
                } break;
                case args_list("ReceiveFrequency"): {
                    auto tmp = args.pop();
                    auto cp  = tmp.find_first_of(':');
                    if (cp != std::string_view::npos) {
                        std::string_view tmp_r = tmp.substr(0, cp);
                        std::string_view tmp_a = tmp.substr(cp + 1);
                        std::from_chars(tmp_r.begin(), tmp_r.end(), rf_r);
                        std::from_chars(tmp_a.begin(), tmp_a.end(), rf_a);
                    }

                } break;
                case args_list("Enter"):
                    is_enter = true;
                    break;
                case args_list("OutputFormat"): {
                    const auto tmp = args.pop();
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
                case args_list("ClockCheckSize"): {
                    const auto tmp = args.pop();
                    std::from_chars(tmp.begin(), tmp.end(), clock_check_size);
                } break;
#endif
                case args_list('h'):
                    args_list.print_arg();
                    exit(0);
                default:
                    fprintf(stderr, "unknown argument: %s\n", args.show().data());
                    exit(1);
            }
        }
    }

    constexpr size_t BUFFER_SIZE = leaky_bucket_buf::NUM_BUFFER;
    // constexpr size_t BUFFER_SIZE = 1360 * 2;
    static leaky_bucket_buf::link_list packet_buffer[BUFFER_SIZE]{};

    UDPReceiver udp(addr.data(), port);
    leaky_bucket_buf buffer(&udp, packet_buffer, BUFFER_SIZE);
    RTPReceiver rtp_recv(&buffer);

    std::chrono::steady_clock::time_point avg_frame;
    size_t analysis_frame          = 0;
    size_t loss_frame              = 0;
    uint32_t frame_lost_precinct   = 0;
    size_t RTP_error_count         = 0;
    size_t J2K_error_count         = 0;
    long double sum_avg            = 0;
    size_t sum_lost_packet         = 0;
    double analysis_operating_time = 0;
    double receive_operating_time  = 0;

    std::atomic_bool analysis_stoper = false;

    std::mutex img_clock_locker;
    std::condition_variable img_clock_cond;
    std::atomic_uint32_t img_inc = 0;
    std::chrono::steady_clock::time_point img_clock;

    auto to_duration = [](const auto t) constexpr { return std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>{t * (1.0 / J2KPayloadHeader_trait::media_clock_Hz)}); };

#ifdef RTP_CLOCK_CHECK
    std::vector<std::chrono::steady_clock::time_point> debug_clock_check{clock_check_size};
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
        std::chrono::steady_clock::time_point analysis_start;
        std::chrono::steady_clock::time_point analysis_finish;
        size_t table_index     = 0;
        uint32_t PID           = 0;
        uint32_t last_sequence = 0;
        auto frame_update      = [&]() {
            if (frame_lost_precinct != 0) {
                const auto lost_per = static_cast<double>(frame_lost_precinct) / ConstValue::all_precinct * 100;
                fprintf(
                    stderr,
                    "    analysis_frame: %ld, lost_precinct: %d/%d, %.6lf%%\n",
                    analysis_frame, frame_lost_precinct, ConstValue::all_precinct, lost_per
                );
                frame_lost_precinct = 0;
            }
            ++analysis_frame;

            img_clock += to_duration(img_inc.load(std::memory_order_acquire));
            std::this_thread::sleep_until(img_clock);
#ifdef RTP_CLOCK_CHECK
            if (debug_clock_it != debug_clock_it_end)
                *debug_clock_it = std::chrono::steady_clock::now();
            ++debug_clock_it;
#endif
            if (out_flame != 0 && analysis_frame % out_flame == 0) {
                auto now = std::chrono::steady_clock::now();
                auto avg = std::chrono::duration_cast<std::chrono::microseconds>(now - avg_frame);
                if (output_format == OutF::FPS) {
                    const auto avg_fps = 1 / ((static_cast<float>(avg.count()) / 1000) / out_flame) * 1000;
                    sum_avg += avg_fps;
                    printf("analysis_frame: %ld, avg: %.6f fps\n", analysis_frame, avg_fps);
                } else if (output_format == OutF::MS) {
                    const auto avg_ms = (static_cast<float>(avg.count()) / out_flame) / 1000;
                    sum_avg += avg_ms;
                    printf("analysis_frame: %ld, avg: %.6f ms\n", analysis_frame, avg_ms);
                }
                avg_frame = now;
            }
        };
        if (unlikely(is_enter)) {
            while (!analysis_stoper);
        }
        printf("analysis thread ready...\n");
        analysis_start = std::chrono::steady_clock::now();

#ifndef DISABLE_TABLE
        MainHeader main_header;
        Tile j2k_tile;
        std::array<fast_table, ConstValue::all_precinct> j2k_packet_table{};
        {
            int32_t result = 0;
            while (true) {
                result = rtp_recv.first_check();
                if (result == RTPReceiver::MAIN_HEADER) break;
                if (result == RTPReceiver::FINISH) {
                    return;
                }
            }
            // avg_frame = std::chrono::steady_clock::now();
            J2kBuf buf(&rtp_recv);
            main_header.read(buf);
            j2k_tile.init(main_header, buf);
            j2k_tile.read(main_header, j2k_packet_table);
            printf("main header read, seq: %d\n", rtp_recv.get_last_sequence_number());
        }
#endif
        if (likely(!is_enter)) {
            std::unique_lock lk{img_clock_locker};
            img_clock_cond.wait(lk);
            img_clock = std::chrono::steady_clock::now();
            avg_frame = img_clock;
        }
        while (!sig_flag) {
#ifdef DISABLE_TABLE
            MainHeader main_header;
            Tile j2k_tile;
            std::array<fast_table, ConstValue::num_precinct * ConstValue::Csiz> j2k_packet_table{};
            {
                while (rtp_recv.check() != RTPReceiver::MAIN_HEADER);
                avg_frame = std::chrono::steady_clock::now();
                J2kBuf buf(&rtp_recv);
                main_header.read(buf);
                j2k_tile.init(main_header, buf);
                j2k_tile.read(main_header, j2k_packet_table);
                printf("main header read, seq: %d\n", rtp_recv.get_last_sequence_number());
            }
            while (true)
#endif
                try {
                    last_sequence          = rtp_recv.get_last_sequence_number();
                    const auto recv_result = rtp_recv.check();
                    if (likely(recv_result == RTPReceiver::SUCCESS)) { // 正常受信
                        PID = rtp_recv.get_PID();
                        J2kBuf buf(&rtp_recv);
                        while (true) {
                            if (j2k_packet_table[table_index].PID == PID) break;
                            j2k_packet_table[table_index].read_packet(buf);
                            ++table_index;
                        }
                        // assert(buf.empty());
                        if (unlikely(rtp_recv.EOC())) { // フレーム終了
                            table_index = 0;
                            frame_update();
                        }
                    } else if (recv_result == RTPReceiver::MAIN_HEADER) { // メインパケット出現
                        ;
                    } else if (recv_result == RTPReceiver::FAILURE) { // パケットロス
                        PID                  = rtp_recv.get_PID();
                        size_t loss_precinct = 0;
                        ++RTP_error_count;
                        while (true) {
                            if (j2k_packet_table[table_index].PID == PID) break;
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
                            "  RTP error analysis_frame: %ld, lost_packet: %d, discarded_packet: %d, lost_precinct: %ld\n",
                            analysis_frame, rtp_recv.get_lost_packet(), rtp_recv.get_last_sequence_number() - last_sequence, loss_precinct
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
                    auto dest_packet  = buffer.dest(
                        [](const uint8_t* const data) -> bool { return static_cast<bool>(J2KPayloadHeader_trait::get_MH(data + RTPHeader_trait::length)); }
                    );
                    fputs(e.what(), stderr);
                    fprintf(stderr, ": buffer leak error analysis_frame: %ld, discarded packsts: %ld, in buf: %ld\n", analysis_frame, dest_packet, in_buf);
                    ++loss_frame;
                    ++J2K_error_count;
                    table_index = 0;
                }
        }
#ifdef DISABLE_TABLE
    long_break:
#endif
        analysis_finish         = std::chrono::steady_clock::now();
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

        std::chrono::steady_clock::time_point receive_start;
        std::chrono::steady_clock::time_point receive_finish;
        uint32_t pre_timestamp = 0;
        uint32_t pre_TPSTAMP   = 0;
        uint32_t pre_flow      = 0;
        const auto pkt_inc_r   = to_duration(rf_r);
        const auto pkt_inc_a   = to_duration(rf_a);

        printf("receive thread ready...\n");
        receive_start    = std::chrono::steady_clock::now();
        auto packet_abs  = receive_start;
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
                    if (!is_img_init && pre_timestamp != 0 && tp != 0) {
                        // std::unique_lock lk{img_clock_locker};
                        img_inc.store(tp - pre_timestamp, std::memory_order_release);
                        img_clock_cond.notify_one();
                        is_img_init = true;
                    }
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
                // メディアクロックとPTSTAMPをもとに待機時間を算出
                packet_abs += pkt_inc_r;
                std::this_thread::sleep_until(packet_abs);
            } else if (result == leaky_bucket_buf::AGAIN) {
                // false: メディアクロックの 1/4 で待機
                packet_abs += pkt_inc_a;
                std::this_thread::sleep_until(packet_abs);
            } else if (result == leaky_bucket_buf::SIGNAL) {
                img_clock_cond.notify_one();
                break;
            } else if (likely(result == leaky_bucket_buf::FINISH)) {
                break;
            }
        }
        receive_finish         = std::chrono::steady_clock::now();
        receive_operating_time = std::chrono::duration_cast<std::chrono::milliseconds>(receive_finish - receive_start).count() / 1000.0;
        if (sig_flag) putc('\n', stdout);
        printf("receive finish\n");
#ifdef GENERATE_RECEIVE_PROBABILITY
        printf("receive: %ld\n", count_receive);
        printf("again:   %ld\n", count_again);
        printf("receive probability: %lf%% \n", static_cast<double>(count_receive) / static_cast<double>(count_receive + count_again) * 100);
#endif
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
    printf("analysis thread's operating time: %lfs\n", analysis_operating_time);
    printf("receive thread's operating time: %lfs\n", receive_operating_time);
    if (output_format == OutF::FPS) {
        printf("average fps: %Lffps\n", sum_avg / (analysis_frame / out_flame));
    } else {
        printf("average fps: %Lfms\n", sum_avg / (analysis_frame / out_flame));
    }
    printf("analysis frame: %ld\n", analysis_frame);
    printf("lost frame: %ld\n", loss_frame);
    printf("lost packets: %ld/%ld\n", sum_lost_packet, analysis_frame * 1360);
    printf("packet loss rate: %lf%%\n", (sum_lost_packet / (analysis_frame * 1360.0)) * 100);
    printf("RTP packet error: %ld\n", RTP_error_count);
    printf("J2K packet error: %ld\n", J2K_error_count);

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