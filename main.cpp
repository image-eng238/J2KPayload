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

#include "codestream.hpp"
#include "decoding.hpp"
#include "const_value.hpp"

#include <vector>
#include <string_view>
#include <charconv>
#include <cassert>
#include <array>

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

    UDPReceiver udprecv;
    if (addr.empty() && port == 0) {
        udprecv.sock_bind();
    } else {
        udprecv.sock_bind(addr.data(), port);
    }

    uint32_t pre_sequence = 0;

    MainHeader main_header;
    Tile j2k_tile;
    std::array<Precinct*, ConstValue::num_precinct * ConstValue::Csiz> j2k_packet_table;

    while (true) {
        uint8_t recv_buf[MAX_PACKET_SIZE] = {};
        // receive UDP packet
        auto pkt_size                     = udprecv.receive(recv_buf, MAX_PACKET_SIZE);
        if (pkt_size == -1) {
            std::cout << "receive error, errno: " << errno << std::endl;
            continue;
        }
        RTPHeader rtp(recv_buf);
        J2KPayloadHeader j2kpayload(&recv_buf[rtp.get_header_length()]);
        uint8_t* pkt_data    = &recv_buf[rtp.get_header_length() + j2kpayload.get_header_length()];
        size_t pkt_data_size = pkt_size - (rtp.get_header_length() + j2kpayload.get_header_length());

        uint32_t extended_sequence_number = (j2kpayload.get_ESEQ() << 16) | rtp.get_sequence_number();
        assert((std::cout << std::dec << "pkt_size: " << pkt_size << ", pkt_data_size: " << pkt_data_size << ", extended_sequence_number:" << extended_sequence_number << std::endl, true));
        assert((extended_sequence_number == pre_sequence + 1) || (pre_sequence == 0));
        pre_sequence = extended_sequence_number;

        // decoder
        if (j2kpayload.get_MH() == 0) { // body packet

        } else if (main_header.empty()) {
            J2kBuf buf(pkt_data);
            main_header.read(buf);
            j2k_tile.init(main_header, buf);
            j2k_tile.read(main_header, j2k_packet_table);
        }
    }

    return 0;
}