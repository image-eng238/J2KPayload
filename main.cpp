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
#include <cstring>

#include <pthread.h>

constexpr size_t MAX_PACKET_SIZE = 1384;

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

    static RTPReceiver rtp_recv;
    if (addr.empty() && port == 0) {
        rtp_recv.sock_bind();
    } else {
        rtp_recv.sock_bind(addr.data(), port);
    }

    std::chrono::steady_clock::time_point avg_frame;
    size_t analysis_frame = 0;
    size_t loss_frame     = 0;
    MainHeader main_header;
    Tile j2k_tile;
    std::once_flag is_main_packet_read;
    std::array<fast_table, ConstValue::num_precinct * ConstValue::Csiz> j2k_packet_table_base;

    auto analysis_process = [&](const size_t t_num) -> void {
        printf("analysis thread ready...\n");
        std::array<fast_table, ConstValue::num_precinct * ConstValue::Csiz> j2k_packet_table;
        RTPReceiver_ref rtp_ref{rtp_recv.access_recv_buf().get_accesser(t_num)};

        avg_frame = std::chrono::steady_clock::now();
        // 一回だけ main packet を解析
        std::call_once(is_main_packet_read, [&]() -> void {
            printf("main header reading(%d)...\n", t_num);
            rtp_ref.pop();
            auto& j2kpayload    = rtp_ref.access_payload();
            auto& pkt_data      = rtp_ref.access_pkt_data_ptr();
            auto& pkt_data_size = rtp_ref.access_pkt_data_size();
            J2kBuf buf(pkt_data, pkt_data_size, &rtp_ref);
            main_header.read(buf);
            j2k_tile.init(main_header, buf);
            j2k_tile.read(main_header, j2k_packet_table_base);
            printf("main header read, seq: %d\n", t_num, rtp_ref.get_extended_sequence_number());
        });
        // 解析済みのテーブルをローカル変数にコピー
        memcpy(&j2k_packet_table, &j2k_packet_table_base, sizeof(j2k_packet_table_base));

        while (true) {
            try {
                if (unlikely(!rtp_ref.pop())) break;
                auto& j2kpayload    = rtp_ref.access_payload();
                auto& pkt_data      = rtp_ref.access_pkt_data_ptr();
                auto& pkt_data_size = rtp_ref.access_pkt_data_size();

                if (likely(j2kpayload.get_MH() == 0)) { // body packet
                    J2kBuf buf(pkt_data, pkt_data_size, &rtp_ref);
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
                    // main packet
                }

            } catch (rtp_sequence_error& e) {
                // メインパケットの出現までパケットを破棄
                // 将来的には timestanp で制御
                auto dest_packet = rtp_ref.dest_packt();
                // fprintf(stderr, "RTP sequence error, pre_seq: %d, seq: %d, lost packets: %d, discarded packsts: %ld, frame: %ld\n", e.pre_sq, e.err_sq, e.err_sq - (e.pre_sq + 1), dest_packet, analysis_frame);
                fprintf(stderr, "RTP error analysis_frame: %ld, lost packets: %d, discarded packsts: %ld\n", analysis_frame, e.err_sq - (e.pre_sq + 1), dest_packet);
                ++loss_frame;
            } catch (J2K_packet_error& e) {
                auto dest_packet = rtp_ref.dest_packt();
                switch (e.type) {
                    case J2K_packet_error::empty_packet:
                        fprintf(stderr, "j2k packet error analysis_frame: %ld, discarded packsts: %ld, data in buf(unsafe): %ld\n", analysis_frame, dest_packet, rtp_ref.get_recv_buf_ptr()->get_num_data_unsafe());
                        break;
                    case J2K_packet_error::segment_byte:
                        fprintf(stderr, "segment error analysis_frame: %ld, discarded packsts: %ld, data in buf(unsafe): %ld\n", analysis_frame, dest_packet, rtp_ref.get_recv_buf_ptr()->get_num_data_unsafe());
                        break;
                    default:
                        fprintf(stderr, "unknown error analysis_frame: %ld, discarded packsts: %ld, data in buf(unsafe): %ld\n", analysis_frame, dest_packet, rtp_ref.get_recv_buf_ptr()->get_num_data_unsafe());
                }
                ++loss_frame;
            }
        }
        printf("analysis finish\n");
    };

    std::thread analysis_t1(analysis_process, 0);
    std::thread analysis_t2(analysis_process, 1);

    auto& r = rtp_recv.access_recv_buf();
    std::thread receive_t([&r]() {
        printf("receive thread ready...\n");
        while (r.receive()) {
            // std::this_thread::yield();
        }
        printf("receive finish\n");
    });

    if (CPU_COUNT(&affinity) != 0) {
        if (auto result = pthread_setaffinity_np(receive_t.native_handle(), sizeof(affinity), &affinity); result != 0) {
            fprintf(stderr, "pthread_setaffinity_up() error: %d\n", result);
            exit(1);
        }
    }

    analysis_t1.join();
    analysis_t2.join();
    receive_t.join();

    printf("analysis frame: %ld\n", analysis_frame);
    printf("lost frame: %ld\n", loss_frame);
#ifdef GENERATE_RECEIVE_PROBABILITY
    printf("receive: %ld\n", leaky_bucket_buf::count_receive);
    printf("again:   %ld\n", leaky_bucket_buf::count_agaein);
    printf("receive probability: %lf%% \n", static_cast<double>(leaky_bucket_buf::count_receive) / static_cast<double>(leaky_bucket_buf::count_receive + leaky_bucket_buf::count_agaein));
#endif
    // printf("pkt_header_true: %ld\n", CodeBlock::pkt_header_true);
    // printf("pkt_header_false: %ld\n", CodeBlock::pkt_header_false);
    // printf("prob: %lf%%\n", static_cast<double>(CodeBlock::pkt_header_true) / (CodeBlock::pkt_header_true + CodeBlock::pkt_header_false));
    // printf("true : %ld\nfalse: %ld\n", J2kBuf::count_true, J2kBuf::count_false);
    // printf("true%% : %lf\n", static_cast<double>(J2kBuf::count_true) / static_cast<double>(J2kBuf::count_true + J2kBuf::count_false));
    // printf("false%%: %lf\n", static_cast<double>(J2kBuf::count_false) / static_cast<double>(J2kBuf::count_true + J2kBuf::count_false));

    return 0;
}
