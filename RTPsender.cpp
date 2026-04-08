#include "UDP.hpp"
#include "RTP_file_format.hpp"
#include "RTP_header.hpp"

#include <vector>
#include <string>
#include <string_view>
#include <charconv>
#include <thread>
#include <chrono>

// ./build/bin/RTPsender -r ./data/2160p_5994fps_422_10bit.rtp -a 127.0.0.1 -p 50001 -s 1 -i 1

// 2160p_5994fps_422_10bit.rtp パケット数は 163200 2秒分のサンプルなので 1フレーム当たりのパケット数は 1360

int main(int argc, char** argv) {

    std::vector<std::string_view> arg_view(argc - 1);
    for (size_t i = 1; i < argc; ++i) {
        arg_view[i - 1] = argv[i];
    }

    std::string_view addr, rtp_path;
    uint16_t port          = 0;
    int64_t interval       = 0;
    size_t skep_frame      = 0;
    int64_t allowable_time = 16;

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
        if (*it == "-r") {
            it++;
            rtp_path = it->data();
        }
        if (*it == "-i") {
            it++;
            if (std::from_chars(it->begin(), it->end(), interval).ptr != it->end()) {
                interval = 0;
            }
        }
        if (*it == "-s") {
            it++;
            if (std::from_chars(it->begin(), it->end(), skep_frame).ptr != it->end()) {
                skep_frame = 0;
            }
        }
        if (*it == "-f") {
            it++;
            if (std::from_chars(it->begin(), it->end(), allowable_time).ptr != it->end()) {
                allowable_time = 16;
            }
        }
    }

    RTPFile rtp(rtp_path.data());
    UDPSender udp(addr.data(), port);
    uint8_t send_buffer[1600] = {};
    uint16_t send_pktsize     = 0;

    size_t now_frame = 0;

    uint32_t pre_num = 0;

    // while (true) {
    //     send_pktsize = rtp.get_packet(send_buffer);
    //     if (send_pktsize == 0) break;
    //     RTPHeader r(send_buffer);
    //     J2KPayloadHeader j(send_buffer + r.get_header_length());
    //     if (j.get_MH() != 0) { // is main packet
    //         ++now_frame;
    //     }
    //     if (now_frame <= skep_frame) {
    //         continue;
    //     }
    //     udp.send(send_buffer, send_pktsize);
    //     std::this_thread::sleep_for(std::chrono::milliseconds(interval));
    // }

    while (true) {
        auto p = std::chrono::system_clock::now() + std::chrono::milliseconds(allowable_time); // 1 ループに 16ms 以上で行うことで 60fps を再現

        send_pktsize = rtp.get_packet(send_buffer);
        if (send_pktsize == 0) break;
        udp.send(send_buffer, send_pktsize);
        now_frame++;

        uint32_t num = (J2KPayloadHeader(RTPHeader(send_buffer).get_header_length() + send_buffer).get_ESEQ() << 16) | RTPHeader(send_buffer).get_sequence_number();
        if (!((num == pre_num + 1) || (pre_num == 0))) {
            std::cout << "assert false" << std::endl;
            exit(1);
        }
        pre_num = num;

        assert(J2KPayloadHeader(RTPHeader(send_buffer).get_header_length() + send_buffer).get_MH());
        while (true) {
            send_pktsize = rtp.get_packet(send_buffer);
            udp.send(send_buffer, send_pktsize);
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));

            uint32_t num = (J2KPayloadHeader(RTPHeader(send_buffer).get_header_length() + send_buffer).get_ESEQ() << 16) | RTPHeader(send_buffer).get_sequence_number();
            if (!((num == pre_num + 1) || (pre_num == 0))) {
                std::cout << "assert false" << std::endl;
                exit(1);
            }
            pre_num = num;

            if (RTPHeader(send_buffer).get_M()) break; // get_M() == true のとき EOF をパケットに含む
        }

        if (now_frame % 10 == 0) {
            printf("frame: %ld\n", now_frame);
        }

        std::this_thread::sleep_until(p);
    }

    return 0;
}