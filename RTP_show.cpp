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
        printf("  V = %d\n", static_cast<uint32_t>(RTPHeader_trait::get_V(pktbuf)));
        printf("  P = %d\n", static_cast<uint32_t>(RTPHeader_trait::get_P(pktbuf)));
        printf("  X = %d\n", static_cast<uint32_t>(RTPHeader_trait::get_X(pktbuf)));
        printf("  CC = %d\n", static_cast<uint32_t>(RTPHeader_trait::get_CC(pktbuf)));
        printf("  M = %d\n", static_cast<uint32_t>(RTPHeader_trait::get_M(pktbuf)));
        printf("  PT = %d\n", static_cast<uint32_t>(RTPHeader_trait::get_PT(pktbuf)));
        printf("  sequence_number = %d\n", static_cast<uint32_t>(RTPHeader_trait::get_sequence_number(pktbuf)));
        printf("  timestamp = %d\n", static_cast<uint32_t>(RTPHeader_trait::get_timestamp(pktbuf)));
        printf("  SSRC = %d\n", static_cast<uint32_t>(RTPHeader_trait::get_SSRC(pktbuf)));
        uint8_t* payload = RTPHeader_trait::length + pktbuf;
        printf("RTP payload header\n");
        printf("  MH = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_MH(payload)));
        printf("  TP = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_TP(payload)));

        if (J2KPayloadHeader_trait::get_MH(payload)) {
            printf("  ORDH = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_ORDH(payload)));
            printf("  P = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_P(payload)));
            printf("  XTRAC = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_XTRAC(payload)));
            printf("  PTSTAMP = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_PTSTAMP(payload)));
            printf("  ESEQ = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_ESEQ(payload)));
            printf("  R = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_R(payload)));
            printf("  S = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_S(payload)));
            printf("  C = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_C(payload)));
            printf("  RSVD = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_RSVD(payload)));
            printf("  RANGE = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_RANGE(payload)));
            printf("  PRIMS = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_PRIMS(payload)));
            printf("  TRANS = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_TRANS(payload)));
            printf("  MAT = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_MAT(payload)));
        } else {
            printf("  RES = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_body_RES(payload)));
            printf("  ORDB = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_body_ORDB(payload)));
            printf("  QUAL = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_body_QUAL(payload)));
            printf("  PTSTAMP = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_body_PTSTAMP(payload)));
            printf("  ESEQ = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_body_ESEQ(payload)));
            printf("  POS = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_body_POS(payload)));
            printf("  PID = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_body_PID(payload)));
        };
    }

    return 0;
}