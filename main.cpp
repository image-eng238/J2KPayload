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

int main(int argc, char** argv) {
    std::vector<std::string_view> arg_view(argc - 1);
    for (size_t i = 1; i < argc; ++i) {
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

    RTPReceiver rtp_recv;
    if (addr.empty() && port == 0) {
        rtp_recv.sock_bind();
    } else {
        rtp_recv.sock_bind(addr.data(), port);
    }

    std::thread consumer([&] {

    MainHeader main_header;
    Tile j2k_tile;
    std::array<Precinct*, ConstValue::num_precinct * ConstValue::Csiz> j2k_packet_table;

    while (true) {
        rtp_recv.receive();
        auto& j2kpayload    = rtp_recv.access_payload();
        auto& pkt_data      = rtp_recv.access_pkt_data_ptr();
        auto& pkt_data_size = rtp_recv.access_pkt_data_size();

        // decoder
        if (j2kpayload.get_MH() == 0) { // body packet
            J2kBuf buf(pkt_data, pkt_data_size, &rtp_recv);
            size_t loop_count = 0;
            for (auto& p : j2k_packet_table) {
                j2k_tile.read_packet(p, buf);
                ptrdiff_t diff = buf.get_ptr() - pkt_data;
                size_t pos     = diff / sizeof(uint8_t);
                if (pos >= MAX_PACKET_SIZE) {
                    break;
                }
                ++loop_count;
            }

        } else if (main_header.empty()) {
            J2kBuf buf(pkt_data, pkt_data_size, &rtp_recv);
            main_header.read(buf);
            j2k_tile.init(main_header, buf);
            j2k_tile.read(main_header, j2k_packet_table);
        }
    } });

    auto& r = rtp_recv.access_recv_buf();
    std::thread produser([&r]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
        while (true) r.receive();
    });

    consumer.join();
    produser.join();

    return 0;
}