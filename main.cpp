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
    std::vector<std::string_view> arg_view(argc - 1);
    for (int i = 1; i < argc; ++i) {
        arg_view[i - 1] = argv[i];
    }

    std::string_view addr;
    uint16_t port    = 0;
    size_t out_flame = 100;
    cpu_set_t affinity;
    CPU_ZERO(&affinity);

    for (auto it = arg_view.begin(); it != arg_view.end(); ++it) {

        if (*it == "-a") {
            it++;
            addr = it->data();
        }
        if (*it == "-p") {
            it++;
            if (std::from_chars(it->begin(), it->end(), port).ptr != it->end()) {
                port = 0;
            }
        }
        if (*it == "-f") {
            it++;
            if (std::from_chars(it->begin(), it->end(), out_flame).ptr != it->end()) {
                out_flame = 100;
            }
        }
        if (*it == "-c") {
            it++;
            size_t cpu_bit_mask = 0;
            if (std::from_chars(it->begin(), it->end(), cpu_bit_mask, 16).ptr == it->end()) {
                size_t i = 0;
                while (cpu_bit_mask != 0) {
                    if (cpu_bit_mask & 0x1)
                        CPU_SET(i, &affinity);
                    cpu_bit_mask >>= 1;
                    ++i;
                }
            }
        }
    }

    RTPReceiver rtp_recv;
    if (addr.empty() && port == 0) {
        rtp_recv.sock_bind();
    } else {
        rtp_recv.sock_bind(addr.data(), port);
    }

    std::chrono::steady_clock::time_point analysis_start;
    std::chrono::steady_clock::time_point analysis_finish;
    std::chrono::steady_clock::time_point receive_start;
    std::chrono::steady_clock::time_point receive_finish;
    std::chrono::steady_clock::time_point avg_frame;
    size_t analysis_frame = 0;
    size_t loss_frame     = 0;

    std::thread consumer([&] {
        MainHeader main_header;
        Tile j2k_tile;
        // std::array<Precinct*, ConstValue::num_precinct * ConstValue::Csiz> j2k_packet_table;
        std::array<fast_table, ConstValue::num_precinct * ConstValue::Csiz> j2k_packet_table;

        analysis_start = std::chrono::steady_clock::now();
        printf("analysis thread ready...\n");
        while (true) {
            try {
                if (unlikely(!rtp_recv.receive())) break;
                auto& j2kpayload    = rtp_recv.access_payload();
                auto& pkt_data      = rtp_recv.access_pkt_data_ptr();
                auto& pkt_data_size = rtp_recv.access_pkt_data_size();

                // decoder
                if (likely(j2kpayload.get_MH() == 0)) { // body packet

                    J2kBuf buf(pkt_data, pkt_data_size, &rtp_recv);
                    size_t loop_count = 0;
                    for (auto& p : j2k_packet_table) {
                        // read_packet(p, buf);
                        p.read_packet(buf);
                        ++loop_count;
                    }

                    // フレーム終了
                    ++analysis_frame;
                    if (out_flame != 0 && analysis_frame % out_flame == 0) {
                        auto now = std::chrono::steady_clock::now();
                        auto avg = std::chrono::duration_cast<std::chrono::microseconds>(now - avg_frame);
                        printf("analysis_frame: %ld, avg: %.4fms, data in buf(unsafe): %ld\n", analysis_frame, (static_cast<float>(avg.count()) / 1'000) / out_flame, rtp_recv.access_recv_buf().get_num_data_unsafe());
                        avg_frame = now;
                    }
                } else {
                    // フレーム開始
                    if (unlikely(main_header.empty())) {
                        avg_frame = std::chrono::steady_clock::now();
                        J2kBuf buf(pkt_data, pkt_data_size, &rtp_recv);
                        main_header.read(buf);
                        j2k_tile.init(main_header, buf);
                        j2k_tile.read(main_header, j2k_packet_table);
                        printf("main header read, seq: %d\n", rtp_recv.get_extended_sequence_number());
                    }
                }
            } catch (rtp_sequence_error& e) {
                // メインパケットの出現までパケットを破棄
                // 将来的には timestanp で制御
                auto dest_packet = rtp_recv.dest_packt();
                // fprintf(stderr, "RTP sequence error, pre_seq: %d, seq: %d, lost packets: %d, discarded packsts: %ld, frame: %ld\n", e.pre_sq, e.err_sq, e.err_sq - (e.pre_sq + 1), dest_packet, analysis_frame);
                fprintf(stderr, "RTP error analysis_frame: %ld, lost packets: %d, discarded packsts: %ld\n", analysis_frame, e.err_sq - (e.pre_sq + 1), dest_packet);
                ++loss_frame;
            } catch (J2K_packet_error& e) {
                auto dest_packet = rtp_recv.dest_packt();
                switch (e.type) {
                    case J2K_packet_error::empty_packet:
                        fprintf(stderr, "j2k packet error analysis_frame: %ld, discarded packsts: %ld, data in buf(unsafe): %ld\n", analysis_frame, dest_packet, rtp_recv.access_recv_buf().get_num_data_unsafe());
                        break;
                    case J2K_packet_error::segment_byte:
                        fprintf(stderr, "segment error analysis_frame: %ld, discarded packsts: %ld, data in buf(unsafe): %ld\n", analysis_frame, dest_packet, rtp_recv.access_recv_buf().get_num_data_unsafe());
                        break;
                    default:
                        fprintf(stderr, "unknown error analysis_frame: %ld, discarded packsts: %ld, data in buf(unsafe): %ld\n", analysis_frame, dest_packet, rtp_recv.access_recv_buf().get_num_data_unsafe());
                }
                ++loss_frame;
            }

            // std::this_thread::yield();
        }
        analysis_finish = std::chrono::steady_clock::now();
        printf("analysis finish: %ld\n", (analysis_finish - analysis_start).count());
    });
    // std::thread consumer([&] {
    //     while (rtp_recv.receive()) {
    //         std::this_thread::yield();
    //     }
    // });

    auto& r = rtp_recv.access_recv_buf();
    std::thread produser([&r, &receive_start, &receive_finish]() {
        receive_start = std::chrono::steady_clock::now();
        printf("receive thread ready...\n");
        // while (true) {
        //     if (!r.receive()) break;
        //     // std::this_thread::sleep_for(std::chrono::microseconds(10));
        // }
        while (r.receive()) {
            // std::this_thread::yield();
        }
        receive_finish = std::chrono::steady_clock::now();
        printf("receive finish: %ld\n", (receive_finish - receive_start).count());
    });

    if (CPU_COUNT(&affinity) != 0) {
        if (auto result = pthread_setaffinity_np(produser.native_handle(), sizeof(affinity), &affinity); result != 0) {
            fprintf(stderr, "pthread_setaffinity_up() error: %d\n", result);
            exit(1);
        }
    }
    // test code
    cpu_set_t test;
    CPU_ZERO(&test);
    CPU_SET(2, &test);
    pthread_setaffinity_np(consumer.native_handle(), sizeof(test), &test);

    consumer.join();
    produser.join();

    auto diff = ((analysis_finish - analysis_start) - (receive_finish - receive_start)).count();
    if (diff < 0) diff *= -1;

    printf("finish diff: %ld\n", diff);
    printf("analysis frame: %ld\n", analysis_frame);
    printf("lost frame: %ld\n", loss_frame);
#ifdef GENERATE_RECEIVE_PROBABILITY
    printf("receive: %ld\n", leaky_bucket_buf::count_receive);
    printf("again:   %ld\n", leaky_bucket_buf::count_agaein);
    printf("receive probability: %lf%% \n", static_cast<double>(leaky_bucket_buf::count_receive) / static_cast<double>(leaky_bucket_buf::count_receive + leaky_bucket_buf::count_agaein));
#endif
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