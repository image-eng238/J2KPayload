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
    size_t file_loop       = 0;

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
        if (*it == "-l") {
            it++;
            if (std::from_chars(it->begin(), it->end(), file_loop).ptr != it->end()) {
                file_loop = 0;
            }
        }
    }

    RTPFile rtp(rtp_path.data());
    UDPSender udp(addr.data(), port);
    uint8_t send_buffer[1600] = {};
    uint16_t send_pktsize     = 0;

    size_t now_frame = 0;

    uint32_t pre_sequence_number = 0;

    uint32_t first_seq       = 0;
    uint32_t diff_seq        = 0;
    size_t current_file_loop = 0;

    size_t sended_packet = 0;

    auto next = std::chrono::system_clock::now(); // 1 ループに 16ms 以上で行うことで 60fps を再現
    while (true) {
        next += std::chrono::milliseconds(allowable_time);

        send_pktsize = rtp.get_packet(send_buffer, diff_seq * current_file_loop); // main packet
        if (send_pktsize == 0) {
            if (current_file_loop == file_loop) {
                break;
            } else {
                if (current_file_loop == 0) diff_seq = pre_sequence_number - first_seq + 1;
                // 2周目以降のための初期設定
                ++current_file_loop;
                rtp.reopen(rtp_path.data());
                send_pktsize = rtp.get_packet(send_buffer, diff_seq * current_file_loop);
            }
        }
        udp.send(send_buffer, send_pktsize);
        sended_packet++;
        now_frame++;

        uint32_t sequence_number = (J2KPayloadHeader(RTPHeader(send_buffer).get_header_length() + send_buffer).get_ESEQ() << 16) | RTPHeader(send_buffer).get_sequence_number();
        if (!((sequence_number == pre_sequence_number + 1) || (pre_sequence_number == 0) || (sequence_number == 0))) {
            std::cout << "assert false sequence_number: " << sequence_number << ", pre_sequence_number: " << pre_sequence_number << std::endl;
            exit(1);
        }
        if (pre_sequence_number == 0) first_seq = sequence_number;
        pre_sequence_number = sequence_number;

        assert(J2KPayloadHeader(RTPHeader(send_buffer).get_header_length() + send_buffer).get_MH());
        while (true) { // body packet
            send_pktsize = rtp.get_packet(send_buffer, diff_seq * current_file_loop);
            udp.send(send_buffer, send_pktsize);
            sended_packet++;
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));

            uint32_t sequence_number = (J2KPayloadHeader(RTPHeader(send_buffer).get_header_length() + send_buffer).get_ESEQ() << 16) | RTPHeader(send_buffer).get_sequence_number();
            if (!((sequence_number == pre_sequence_number + 1) || (pre_sequence_number == 0) || (sequence_number == 0))) {
                std::cout << "assert false sequence_number: " << sequence_number << ", pre_sequence_number: " << pre_sequence_number << std::endl;
                exit(1);
            }
            pre_sequence_number = sequence_number;

            if (RTPHeader(send_buffer).get_M()) break; // get_M() == true のとき EOF をパケットに含む
        }

        if (now_frame % 10 == 0) {
            printf("frame: %ld\n", now_frame);
        }

        std::this_thread::sleep_until(next);
    }
    char stop = 0;
    udp.send(&stop, 1);
    printf("sended_pakcet: %ld\n", sended_packet);

    return 0;
}