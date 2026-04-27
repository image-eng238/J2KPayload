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

#include <vector>
#include <string_view>
#include <charconv>
#include <cassert>
#include <array>
#include <thread>

constexpr size_t MAX_PACKET_SIZE = 1384;

int read_packet(const Precinct* const current_precinct, J2kBuf& payload_buf);

int main(int argc, char** argv) {
    std::vector<std::string_view> arg_view(argc - 1);
    for (int i = 1; i < argc; ++i) {
        arg_view[i - 1] = argv[i];
    }

    std::string_view addr;
    uint16_t port = 0;
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
    }

    static RTPReceiver rtp_recv;
    if (addr.empty() && port == 0) {
        rtp_recv.sock_bind();
    } else {
        rtp_recv.sock_bind(addr.data(), port);
    }

    std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point analysis_finish;
    std::chrono::steady_clock::time_point receive_finish;
    size_t analysis_frame = 0;
    size_t loss_frame     = 0;

    std::thread consumer([&] {
        MainHeader main_header;
        Tile j2k_tile;
        std::array<Precinct*, ConstValue::num_precinct * ConstValue::Csiz> j2k_packet_table;

        printf("analysis thread ready\n");
        while (true) {
            try {
                if (!rtp_recv.receive()) break;
                auto& j2kpayload    = rtp_recv.access_payload();
                auto& pkt_data      = rtp_recv.access_pkt_data_ptr();
                auto& pkt_data_size = rtp_recv.access_pkt_data_size();

                // decoder
                if (j2kpayload.get_MH() == 0) { // body packet

                    J2kBuf buf(pkt_data, pkt_data_size, &rtp_recv);
                    size_t loop_count = 0;
                    for (auto& p : j2k_packet_table) {
                        read_packet(p, buf);
                        ++loop_count;
                    }

                } else {
                    if (main_header.empty()) {
                        J2kBuf buf(pkt_data, pkt_data_size, &rtp_recv);
                        main_header.read(buf);
                        j2k_tile.init(main_header, buf);
                        j2k_tile.read(main_header, j2k_packet_table);
                        printf("main header read\n");
                    }
                    ++analysis_frame;
#ifdef GENERATE_FRAME
                    if (analysis_frame % 100 == 0) {
                        printf("analysis_frame: %ld\n", analysis_frame);
                    }
#endif
                }
            } catch (rtp_sequence_error& e) {
                // メインパケットの出現までパケットを破棄
                // 将来的には timestanp で制御
                while (rtp_recv.dest_packt()) {
                    std::this_thread::yield();
                }
                ++loss_frame;
            }

            std::this_thread::yield();
        }
        analysis_finish = std::chrono::steady_clock::now();
        printf("analysis finish: %ld\n", analysis_finish - start_time);
    });
    // std::thread consumer([&] {
    //     while (rtp_recv.receive()) {
    //         std::this_thread::yield();
    //     }
    // });

    auto& r = rtp_recv.access_recv_buf();
    std::thread produser([&r, &receive_finish, &start_time]() {
        // std::this_thread::sleep_for(std::chrono::milliseconds(10));
        printf("receive thread ready\n");
        // while (true) {
        //     if (!r.receive()) break;
        //     // std::this_thread::sleep_for(std::chrono::microseconds(10));
        // }
        while (r.receive()) {
            // std::this_thread::yield();
        }
        receive_finish = std::chrono::steady_clock::now();
        printf("receive finish: %ld\n", receive_finish - start_time);
    });

    consumer.join();
    produser.join();

    printf("finish diff: %ld\n", analysis_finish - receive_finish);
    printf("analysis frame: %ld\n", analysis_frame);

    return 0;
}

int read_packet(const Precinct* const current_precinct, J2kBuf& payload_buf) {
    static size_t call_count = 0;
    call_count++;
    if (!payload_buf.get_bit()) { // empty packet
        std::cout << "empty packet, call_count: " << call_count << std::endl;
        return 1;
    }

    PrecinctSubband* current_ps;
    for (uint8_t i = 0; i < current_precinct->get_number_of_subband(); ++i) {
        current_ps = current_precinct->get_psubband_ptr(i);
#ifdef GENERATE_LOG
        current_ps->read_packet_header(&payload_buf, current_precinct->get_resolution_level());
#else
        current_ps->read_packet_header(&payload_buf);
#endif
    }
    payload_buf.check_FF();
    payload_buf.r_fill();

    for (uint8_t i = 0; i < current_precinct->get_number_of_subband(); ++i) {
        current_ps = current_precinct->get_psubband_ptr(i);
        current_ps->get_codeblock_ptr(0)->set_data(&payload_buf);
    }
    return 0;
}