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

constexpr size_t MAX_PACKET_SIZE = 1384;

int read_packet(const Precinct* const current_precinct, J2kBuf& payload_buf);

int main(int argc, char** argv) {
    std::string_view addr = "127.0.0.1";
    uint16_t port         = 50001;
    size_t out_flame      = 60;
    cpu_set_t affinity;
    CPU_ZERO(&affinity);
    cpu_set_t affinity_analysis;
    CPU_ZERO(&affinity_analysis);
    bool is_enter = false;
    enum class OutF : uint8_t {
        FPS,
        MS
    };
    OutF output_format = OutF::FPS;

    {
        using namespace tklib;
        static constexpr argument_list args_list(
            {{'a', "address", "IPv4 address default: 127.0.0.1"},
             {'p', "port", "Port default: 50001"},
             {'f', "frame", "The interval between frames to display default: 60"},
             {'c', "receive_affinity", "CPU affinity of the receive thread"},
             {'C', "analysis_affinity", "CPU affinity of the analysis thread"},
             {0, "Enter", "analysis thread continue at enter"},
             {0, "OutputFormat", "this option is determines the output format for the frame rate. value: fps or ms, default: fps"},
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
                case args_list('h'):
                    args_list.print_arg();
                    exit(0);
                default:
                    fprintf(stderr, "unknown argument: %s\n", args.show().data());
                    exit(1);
            }
        }
    }

    UDPReceiver udp(addr.data(), port);
    leaky_bucket_buf buffer(&udp);
    RTPReceiver rtp_recv(&buffer);

    std::chrono::steady_clock::time_point analysis_start;
    std::chrono::steady_clock::time_point analysis_finish;
    std::chrono::steady_clock::time_point receive_start;
    std::chrono::steady_clock::time_point receive_finish;
    std::chrono::steady_clock::time_point avg_frame;
    size_t analysis_frame = 0;
    size_t loss_frame     = 0;

    size_t RTP_error_count = 0;
    size_t J2K_error_count = 0;

    std::atomic_bool analysis_stoper = false;

    uint32_t frame_lost_precinct = 0;

    auto print_frame = [&]() {
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
        if (out_flame != 0 && analysis_frame % out_flame == 0) {
            auto now = std::chrono::steady_clock::now();
            auto avg = std::chrono::duration_cast<std::chrono::microseconds>(now - avg_frame);
            if (output_format == OutF::FPS) {
                const auto avg_fps = 1 / ((static_cast<float>(avg.count()) / 1000) / out_flame) * 1000;
                printf("analysis_frame: %ld, avg: %.6f fps\n", analysis_frame, avg_fps);
            } else if (output_format == OutF::MS) {
                printf("analysis_frame: %ld, avg: %.6f ms\n", analysis_frame, (static_cast<float>(avg.count()) / out_flame) / 1000);
            }
            avg_frame = now;
        }
    };

    std::thread consumer([&] {
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
            while (rtp_recv.check() != RTPReceiver::MAIN_HEADER);
            avg_frame = std::chrono::steady_clock::now();
            J2kBuf buf(&rtp_recv);
            main_header.read(buf);
            j2k_tile.init(main_header, buf);
            j2k_tile.read(main_header, j2k_packet_table);
            printf("main header read, seq: %d\n", rtp_recv.get_last_sequence_number());
        }
#endif

        size_t table_index = 0;
        uint32_t PID       = 0;

        uint32_t last_sequence = 0;
        while (true) {
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
                            print_frame();
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
                                print_frame();
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
                    } else {
#ifdef DISABLE_TABLE
                        goto long_break;
#endif
                        break;
                    }

                } catch (buffer_leak& e) {
                    buffer.clear();
                    auto dest_packet = buffer.dest(
                        [](const uint8_t* const data) -> bool { return static_cast<bool>(J2KPayloadHeader_trait::get_MH(data + RTPHeader_trait::length)); }
                    );
                    fputs(e.what(), stderr);
                    fprintf(stderr, ": buffer leak error analysis_frame: %ld, discarded packsts: %ld\n", analysis_frame, dest_packet);
                    ++loss_frame;
                    ++J2K_error_count;
                    table_index = 0;
                }
        }
#ifdef DISABLE_TABLE
    long_break:
#endif
        analysis_finish = std::chrono::steady_clock::now();
        printf("analysis finish: %ld\n", (analysis_finish - analysis_start).count());
    });

    std::thread produser([&buffer, &receive_start, &receive_finish]() {
#ifdef GENERATE_RECEIVE_PROBABILITY
        size_t count_receive = 0;
        size_t count_again   = 0;
#endif

        receive_start  = std::chrono::steady_clock::now();
        const auto T   = std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>{1.0 / (90 * 1000)});
        const auto T_2 = std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>{0.25 / (90 * 1000)});
        auto abs_time  = std::chrono::steady_clock::now();
        printf("receive thread ready...\n");
        while (true) {
            const auto result = buffer.receive();
            if (result == leaky_bucket_buf::AGAIN) {
#ifdef GENERATE_RECEIVE_PROBABILITY
                ++count_again;
#endif
                abs_time += T_2;
                std::this_thread::sleep_until(abs_time);
                continue;
            };
            if (result == leaky_bucket_buf::RECEIVED) {
#ifdef GENERATE_RECEIVE_PROBABILITY
                ++count_receive;
#endif
                abs_time += T_2;
                std::this_thread::sleep_until(abs_time);
                continue;
            } else {
                break;
            }
        }
        receive_finish = std::chrono::steady_clock::now();
        printf("receive finish: %ld\n", (receive_finish - receive_start).count());
#ifdef GENERATE_RECEIVE_PROBABILITY
        printf("receive: %ld\n", count_receive);
        printf("again:   %ld\n", count_again);
        printf("receive probability: %lf%% \n", static_cast<double>(count_receive) / static_cast<double>(count_receive + count_again));
#endif
    });

    if (CPU_COUNT(&affinity) != 0) {
        if (auto result = pthread_setaffinity_np(produser.native_handle(), sizeof(affinity), &affinity); result != 0) {
            fprintf(stderr, "pthread_setaffinity_up() error: %d\n", result);
            exit(1);
        }
    }
    if (CPU_COUNT(&affinity_analysis) != 0) {
        if (auto result = pthread_setaffinity_np(consumer.native_handle(), sizeof(affinity_analysis), &affinity_analysis); result != 0) {
            fprintf(stderr, "pthread_setaffinity_up() error: %d\n", result);
            exit(1);
        }
    }

    if (unlikely(is_enter)) {
        printf("Press Enter to continue\n");
        getc(stdin);
        analysis_stoper = true;
    }

    consumer.join();
    produser.join();

    auto diff = ((analysis_finish - analysis_start) - (receive_finish - receive_start)).count();
    if (diff < 0) diff *= -1;

    printf("finish diff: %ld\n", diff);
    printf("analysis frame: %ld\n", analysis_frame);
    printf("lost frame: %ld\n", loss_frame);
    printf("RTP packet error: %ld\n", RTP_error_count);
    printf("J2K packet error: %ld\n", J2K_error_count);
    // printf("pkt_header_true: %ld\n", CodeBlock::pkt_header_true);
    // printf("pkt_header_false: %ld\n", CodeBlock::pkt_header_false);
    // printf("prob: %lf%%\n", static_cast<double>(CodeBlock::pkt_header_true) / (CodeBlock::pkt_header_true + CodeBlock::pkt_header_false));
    // printf("true : %ld\nfalse: %ld\n", J2kBuf::count_true, J2kBuf::count_false);
    // printf("true%% : %lf\n", static_cast<double>(J2kBuf::count_true) / static_cast<double>(J2kBuf::count_true + J2kBuf::count_false));
    // printf("false%%: %lf\n", static_cast<double>(J2kBuf::count_false) / static_cast<double>(J2kBuf::count_true + J2kBuf::count_false));

    return 0;
}

int read_packet(const Precinct* const current_precinct, J2kBuf& payload_buf) {
    // Precinct::get_number_of_subband() のメモリアクセスがボトルネック
    // 実際には current_precinct の実体がキャッシュに乗っていないため，
    // 一回目のアクセスに時間がかかる
    static size_t call_count = 0;
    call_count++;
    if (unlikely(!payload_buf.get_bit())) { // empty packet
        std::cout << "empty packet, call_count: " << call_count << std::endl;
        return 1;
    }

    PrecinctSubband* current_ps;
    const uint8_t num_subband = current_precinct->get_number_of_subband();
    assume(num_subband <= 3);
    for (uint8_t i = 0; i < num_subband; ++i) {
        current_ps = current_precinct->get_psubband_ptr(i);
#ifdef GENERATE_LOG
        current_ps->read_packet_header(&payload_buf, current_precinct->get_resolution_level());
#else
        current_ps->read_packet_header(&payload_buf);
#endif
    }
    payload_buf.check_FF();
    payload_buf.r_fill();

    for (uint8_t i = 0; i < num_subband; ++i) {
        current_ps = current_precinct->get_psubband_ptr(i);
        current_ps->get_codeblock_ptr(0)->set_data(&payload_buf);
    }
    return 0;
}
