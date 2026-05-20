#include "RTP_file_format.hpp"
#include "RTP_header.hpp"

#include <vector>
#include <string_view>
#include <cstdio>

int main(int argc, char** argv) {
    std::vector<std::string_view> arg_view(argc - 1);
    for (int i = 1; i < argc; ++i) {
        arg_view[i - 1] = argv[i];
    }

    std::string_view rtp_path;

    for (auto it = arg_view.begin(); it != arg_view.end(); ++it) {
        if (*it == "-r") {
            it++;
            rtp_path = it->data();
        }
    }

    uint16_t pktsize     = 0;
    uint8_t pktbuf[1500] = {};
    size_t num_packet    = 0;
    size_t num_frame     = 0;
    RTPFile rtp(rtp_path.data());
    while (true) {
        pktsize = rtp.get_packet(pktbuf);
        printf("pakcet[%ld]\n", num_packet);
        printf("packet size = %d\n", pktsize);
        printf("RTP packet header\n");
        printf("  V = \n", static_cast<uint32_t>(RTPHeader_trait::get_V(pktbuf)));
        printf("  P = \n", static_cast<uint32_t>(RTPHeader_trait::get_P(pktbuf)));
        printf("  X = \n", static_cast<uint32_t>(RTPHeader_trait::get_X(pktbuf)));
        printf("  CC = \n", static_cast<uint32_t>(RTPHeader_trait::get_CC(pktbuf)));
        printf("  M = \n", static_cast<uint32_t>(RTPHeader_trait::get_M(pktbuf)));
        printf("  PT = \n", static_cast<uint32_t>(RTPHeader_trait::get_PT(pktbuf)));
        printf("  sequence_number = \n", static_cast<uint32_t>(RTPHeader_trait::get_sequence_number(pktbuf)));
        printf("  timestamp = \n", static_cast<uint32_t>(RTPHeader_trait::get_timestamp(pktbuf)));
        printf("  SSRC = \n", static_cast<uint32_t>(RTPHeader_trait::get_SSRC(pktbuf)));
        printf("RTP payload header\n");
    }

    return 0;
}