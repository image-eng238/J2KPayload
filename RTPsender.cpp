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

int main(int argc, char** argv) {

    std::vector<std::string_view> arg_view(argc - 1);
    for (size_t i = 1; i < argc; ++i) {
        arg_view[i - 1] = argv[i];
    }

    std::string_view addr, rtp_path;
    uint16_t port     = 0;
    int64_t interval  = 0;
    size_t skep_frame = 0;

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
    }

    RTPFile rtp(rtp_path.data());
    UDPSender udp(addr.data(), port);
    uint8_t send_buffer[1600] = {};
    uint16_t send_pktsize     = 0;

    size_t now_frame = 0;

    while (true) {
        send_pktsize = rtp.get_packet(send_buffer);
        if (send_pktsize == 0) break;
        RTPHeader r(send_buffer);
        J2KPayloadHeader j(send_buffer + r.get_header_length());
        if (j.get_MH() != 0) { // is main packet
            ++now_frame;
        }
        if (now_frame <= skep_frame) {
            continue;
        }
        udp.send(send_buffer, send_pktsize);
        std::this_thread::sleep_for(std::chrono::milliseconds(interval));
    }

    return 0;
}