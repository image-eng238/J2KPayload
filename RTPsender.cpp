#include "UDP.hpp"
#include "RTP_file_format.hpp"

#include <vector>
#include <string>
#include <string_view>
#include <charconv>

int main(int argc, char** argv) {

    std::vector<std::string_view> arg_view(argc - 1);
    for (size_t i = 1; i < argc; ++i) {
        arg_view[i - 1] = argv[i];
    }

    std::string_view addr, rtp_path;
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
        if (*it == "-r") {
            it++;
            rtp_path = it->data();
        }
    }

    RTPFile rtp(rtp_path.data());
    UDPSender udp(addr.data(), port);
    uint8_t send_buffer[1600] = {};
    uint16_t send_pktsize     = 0;

    while (true) {
        send_pktsize = rtp.get_packet(send_buffer);
        if (send_pktsize == 0) break;
        udp.send(send_buffer, send_pktsize);
    }

    return 0;
}