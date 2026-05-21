#pragma once

#include <cstdint>
#include <cstddef>
#include <cassert>
#include <cstdio>
#include <exception>
#include <thread>
#define PRINT_ASSERTION(expr, msg, ...) assert(((expr) ? true : (printf("assertion message: " msg, __VA_ARGS__), false)))

#include "UDP.hpp"
#include "buffer_pool.hpp"
#include "leaky_bucket_buf.hpp"
#include "opt_macro.hpp"

struct rtp_sequence_error {
    uint32_t pre_sq;
    uint32_t err_sq;
};

namespace RTPHeader_trait {
    inline constexpr uint8_t length = 12;
    inline constexpr uint8_t get_header_length() { return RTPHeader_trait::length; }
    inline constexpr uint8_t get_V(const uint8_t* const pointer) { return (pointer[0] & 0xC0) >> 6; };                                                    // version: 2 bits 0b10で固定
    inline constexpr uint8_t get_P(const uint8_t* const pointer) { return (pointer[0] & 0x20) >> 5; };                                                    // padding: 1 bit
    inline constexpr uint8_t get_X(const uint8_t* const pointer) { return (pointer[0] & 0x10) >> 4; };                                                    // extension: 1 bit
    inline constexpr uint8_t get_CC(const uint8_t* const pointer) { return pointer[0] & 0x0F; };                                                          // CSRC_count: 4 bits
    inline constexpr uint8_t get_M(const uint8_t* const pointer) { return (pointer[1] & 0x80) >> 7; };                                                    // marker: 1 bit
    inline constexpr uint8_t get_PT(const uint8_t* const pointer) { return pointer[1] & 0x7F; };                                                          // payload_type: 7 bits
    inline constexpr uint16_t get_sequence_number(const uint8_t* const pointer) { return pointer[2] << 8 | pointer[3]; };                                 // 16 bits
    inline constexpr uint32_t get_timestamp(const uint8_t* const pointer) { return pointer[4] << 24 | pointer[5] << 16 | pointer[6] << 8 | pointer[7]; }; // 32 bits
    inline constexpr uint32_t get_SSRC(const uint8_t* const pointer) { return pointer[8] << 24 | pointer[9] << 16 | pointer[10] << 8 | pointer[11]; };    // 32 bits
}
class RTPHeader {
public:
    RTPHeader() = default;
    RTPHeader(const uint8_t* const ptr) : pointer{ptr} {}

    void set_ptr(const uint8_t* const ptr) { pointer = ptr; }

    uint8_t get_V() const { return RTPHeader_trait::get_V(pointer); };                             // version: 2 bits 0b10で固定
    uint8_t get_P() const { return RTPHeader_trait::get_P(pointer); };                             // padding: 1 bit
    uint8_t get_X() const { return RTPHeader_trait::get_X(pointer); };                             // extension: 1 bit
    uint8_t get_CC() const { return RTPHeader_trait::get_CC(pointer); };                           // CSRC_count: 4 bits
    uint8_t get_M() const { return RTPHeader_trait::get_M(pointer); };                             // marker: 1 bit
    uint8_t get_PT() const { return RTPHeader_trait::get_PT(pointer); };                           // payload_type: 7 bits
    uint16_t get_sequence_number() const { return RTPHeader_trait::get_sequence_number(pointer); } // 16 bits
    uint32_t get_timestamp() const { return RTPHeader_trait::get_timestamp(pointer); };            // 32 bits
    uint32_t get_SSRC() const { return RTPHeader_trait::get_SSRC(pointer); };                      // 32 bits
    static constexpr uint8_t get_header_length() { return RTPHeader_trait::get_header_length(); }

private:
    const uint8_t* pointer;
};
namespace J2KPayloadHeader_trait {
    constexpr uint8_t length = 8;
    inline constexpr uint8_t get_header_length() { return length; }
    inline constexpr uint8_t get_MH(const uint8_t* const pointer) { return (pointer[0] & 0xC0) >> 6; }                    // Codestream Main Header Presence: 2 bits
    inline constexpr uint8_t get_TP(const uint8_t* const pointer) { return pointer[0] & 0x38; }                           // Image Type: 3 bits
    inline constexpr uint16_t get_PTSTAMP(const uint8_t* const pointer) { return (pointer[1] & 0x0F) << 8 | pointer[2]; } // Precision Timestamp: 12 bits
    inline constexpr uint8_t get_ESEQ(const uint8_t* const pointer) { return pointer[3]; }                                // Extended Sequence Number High-Order Bits: 8 bits
                                                                                                                          // main
    inline constexpr uint8_t get_main_ORDH(const uint8_t* const pointer) { return pointer[0] & 0x07; }                    // Progression Order Flag, Main Packet: 3 bits
    inline constexpr uint8_t get_main_P(const uint8_t* const pointer) { return (pointer[1] & 0x80) >> 7; }                // Precision Timestamp Presence: 1 bit
    inline constexpr uint8_t get_main_XTRAC(const uint8_t* const pointer) { return (pointer[1] & 0x70) >> 4; }            // Extension Payload Length: 3 bits
    inline constexpr uint16_t get_main_PTSTAMP(const uint8_t* const pointer) { return get_PTSTAMP(pointer); }             // Precision Timestamp: 12 bits
    inline constexpr uint8_t get_main_ESEQ(const uint8_t* const pointer) { return get_ESEQ(pointer); }                    // Extended Sequence Number High-Order Bits: 8 bits

    inline constexpr uint8_t get_main_R(const uint8_t* const pointer) { return (pointer[4] & 0x80) >> 7; }    // Codestream Main Header Reuse: 1 bit
    inline constexpr uint8_t get_main_S(const uint8_t* const pointer) { return pointer[4] & (0x40) >> 6; }    // Parameterized Colorspace Presence: 1 bit
    inline constexpr uint8_t get_main_C(const uint8_t* const pointer) { return pointer[4] & (0x20) >> 5; }    // Code-Block Caching Usage: 1 bit
    inline constexpr uint8_t get_main_RSVD(const uint8_t* const pointer) { return (pointer[4] & 0x1E) >> 1; } // Reserved: 4 bits
    inline constexpr uint8_t get_main_RANGE(const uint8_t* const pointer) { return pointer[4] & 0x01; }       // Video Full Range Usage: 1 bit
    inline constexpr uint8_t get_main_PRIMS(const uint8_t* const pointer) { return pointer[5]; }              // Color Primaries: 8 bits
    inline constexpr uint8_t get_main_TRANS(const uint8_t* const pointer) { return pointer[6]; }              // Transfer Characteristice: 8 bits
    inline constexpr uint8_t get_main_MAT(const uint8_t* const pointer) { return pointer[7]; }                // Colo Matrix Coefficients: 8 bits
                                                                                                              // body
    inline constexpr uint8_t get_body_RES(const uint8_t* const pointer) { return pointer[0] & 0x07; }         // Resolution Levels: 3 bits
    inline constexpr uint8_t get_body_ORDB(const uint8_t* const pointer) { return (pointer[1] & 0x80) >> 7; } // Progression Order Flag, Body Packet: 1 bit is_resync_point
    inline constexpr uint8_t get_body_QUAL(const uint8_t* const pointer) { return (pointer[1] & 0x70) >> 4; } // Quality Layers: 3 bit
    inline constexpr uint16_t get_body_PTSTAMP(const uint8_t* const pointer) { return get_PTSTAMP(pointer); } // Precision Timestamp: 12 bits
    inline constexpr uint8_t get_body_ESEQ(const uint8_t* const pointer) { return get_ESEQ(pointer); }        // Extended Sequence Number High-Order Bits: 8 bits

    inline constexpr uint16_t get_body_POS(const uint8_t* const pointer) { return (pointer[4] << 4) | ((pointer[5] & 0xF0) >> 4); }               // Resyns Point Offset: 12 bits
    inline constexpr uint32_t get_body_PID(const uint8_t* const pointer) { return (pointer[5] & 0x0F << 16) | (pointer[6] << 8) | (pointer[7]); } // Precinct Identifier: 20 bits

    inline constexpr uint32_t get_extended_sequence_number(const uint8_t* const pointer) { return (get_ESEQ(pointer + RTPHeader_trait::length) << 16) | RTPHeader_trait::get_sequence_number(pointer); }

}

class J2KPayloadHeader {
public:
    J2KPayloadHeader() = default;
    J2KPayloadHeader(const uint8_t* const ptr) : pointer{ptr} {}

    void set_ptr(const uint8_t* const ptr) { pointer = ptr; }

    uint8_t get_MH() const { return J2KPayloadHeader_trait::get_MH(pointer); }            // Codestream Main Header Presence: 2 bits
    uint8_t get_TP() const { return J2KPayloadHeader_trait::get_TP(pointer); }            // Image Type: 3 bits
    uint16_t get_PTSTAMP() const { return J2KPayloadHeader_trait::get_PTSTAMP(pointer); } // Precision Timestamp: 12 bits
    uint8_t get_ESEQ() const { return J2KPayloadHeader_trait::get_ESEQ(pointer); }        // Extended Sequence Number High-Order Bits: 8 bits
    static constexpr uint8_t get_header_length() { return J2KPayloadHeader_trait::get_header_length(); }

    // main
    uint8_t get_main_ORDH() const { return J2KPayloadHeader_trait::get_main_ORDH(pointer); }        // Progression Order Flag, Main Packet: 3 bits
    uint8_t get_main_P() const { return J2KPayloadHeader_trait::get_main_P(pointer); }              // Precision Timestamp Presence: 1 bit
    uint8_t get_main_XTRAC() const { return J2KPayloadHeader_trait::get_main_XTRAC(pointer); }      // Extension Payload Length: 3 bits
    uint16_t get_main_PTSTAMP() const { return J2KPayloadHeader_trait::get_main_PTSTAMP(pointer); } // Precision Timestamp: 12 bits
    uint8_t get_main_ESEQ() const { return J2KPayloadHeader_trait::get_main_ESEQ(pointer); }        // Extended Sequence Number High-Order Bits: 8 bits

    uint8_t get_main_R() const { return J2KPayloadHeader_trait::get_main_R(pointer); }         // Codestream Main Header Reuse: 1 bit
    uint8_t get_main_S() const { return J2KPayloadHeader_trait::get_main_S(pointer); }         // Parameterized Colorspace Presence: 1 bit
    uint8_t get_main_C() const { return J2KPayloadHeader_trait::get_main_C(pointer); }         // Code-Block Caching Usage: 1 bit
    uint8_t get_main_RSVD() const { return J2KPayloadHeader_trait::get_main_RSVD(pointer); }   // Reserved: 4 bits
    uint8_t get_main_RANGE() const { return J2KPayloadHeader_trait::get_main_RANGE(pointer); } // Video Full Range Usage: 1 bit
    uint8_t get_main_PRIMS() const { return J2KPayloadHeader_trait::get_main_PRIMS(pointer); } // Color Primaries: 8 bits
    uint8_t get_main_TRANS() const { return J2KPayloadHeader_trait::get_main_TRANS(pointer); } // Transfer Characteristice: 8 bits
    uint8_t get_main_MAT() const { return J2KPayloadHeader_trait::get_main_MAT(pointer); }     // Colo Matrix Coefficients: 8 bits

    // body
    uint8_t get_body_RES() const { return J2KPayloadHeader_trait::get_body_RES(pointer); }          // Resolution Levels: 3 bits
    uint8_t get_body_ORDB() const { return J2KPayloadHeader_trait::get_body_ORDB(pointer); }        // Progression Order Flag, Body Packet: 1 bit is_resync_point
    uint8_t get_body_QUAL() const { return J2KPayloadHeader_trait::get_body_QUAL(pointer); }        // Quality Layers: 3 bit
    uint16_t get_body_PTSTAMP() const { return J2KPayloadHeader_trait::get_body_PTSTAMP(pointer); } // Precision Timestamp: 12 bits
    uint8_t get_body_ESEQ() const { return J2KPayloadHeader_trait::get_body_ESEQ(pointer); }        // Extended Sequence Number High-Order Bits: 8 bits

    uint16_t get_body_POS() const { return J2KPayloadHeader_trait::get_body_POS(pointer); } // Resyns Point Offset: 12 bits
    uint32_t get_body_PID() const { return J2KPayloadHeader_trait::get_body_PID(pointer); } // Precinct Identifier: 20 bits

private:
    const uint8_t* pointer;
    static constexpr uint8_t length = 8;
};

class RTPReceiver {
public:
    RTPReceiver() : udp{}, rtp_header{}, payload_header{}, pre_sequence_number{}, use_buf{}, recv_buf(&udp) {};
    RTPReceiver(const char* const address, const uint16_t port)
        : udp{address, port}, rtp_header{}, payload_header{}, pre_sequence_number{}, use_buf{}, recv_buf(&udp) {}

    RTPHeader& access_rtp() { return rtp_header; }
    J2KPayloadHeader& access_payload() { return payload_header; }
    uint8_t*& access_pkt_data_ptr() { return pkt_data_ptr; }
    size_t& access_pkt_data_size() { return pkt_data_size; }
    leaky_bucket_buf& access_recv_buf() { return recv_buf; }

    uint8_t* get_use_buf() const { return use_buf; }
    uint32_t get_extended_sequence_number() const { return (payload_header.get_ESEQ() << 16) | rtp_header.get_sequence_number(); }
    static uint32_t get_extended_sequence_number(const uint8_t* const data) { return static_cast<uint32_t>(data[15] << 0x10) | static_cast<uint32_t>(data[2] << 0x8) | static_cast<uint32_t>(data[3]); }

    bool sock_bind() { return udp.sock_bind(); }
    bool sock_bind(const char* const address, const uint16_t port) { return udp.sock_bind(address, port); }

    bool receive() {

        auto pkt_size = recv_buf.pop(use_buf);

        if (unlikely(!(use_buf[0] & 0x80))) { return false; }

        this->rtp_header.set_ptr(use_buf);
        this->payload_header.set_ptr(use_buf + rtp_header.get_header_length());

        uint32_t extended_sequence_number = get_extended_sequence_number();

        if (likely((extended_sequence_number == pre_sequence_number + 1) || (pre_sequence_number == 0) || (extended_sequence_number == 0))) {
            pkt_data_ptr        = use_buf + rtp_header.get_header_length() + payload_header.get_header_length();
            pkt_data_size       = pkt_size - (rtp_header.get_header_length() + payload_header.get_header_length());
            pre_sequence_number = extended_sequence_number;

            return true;
        } else {
            // fprintf(stderr, "RTP sequence error, pre_seq: %d, seq: %d, lost packets: %d, ", pre_sequence_number, extended_sequence_number, extended_sequence_number - (pre_sequence_number + 1));
            rtp_sequence_error err;
            err.pre_sq          = pre_sequence_number;
            err.err_sq          = extended_sequence_number;
            pkt_data_ptr        = nullptr;
            pkt_data_size       = 0;
            pre_sequence_number = extended_sequence_number;

            // std::this_thread::yield();
            throw err;
        }
    }
    size_t dest_packet() {
        // auto dest_packet    = recv_buf.dest([](const uint8_t* const data) { return static_cast<bool>(data[RTPHeader::get_header_length()] & 0xC0); });
        // pre_sequence_number = 0;
        auto dest_packet = recv_buf.dest(
            [](const uint8_t* const data) -> bool { return static_cast<bool>(data[RTPHeader::get_header_length()] & 0xC0); },
            [this](const uint8_t* const data) -> void { this->pre_sequence_number = RTPReceiver::get_extended_sequence_number(data); }
        );
        // fprintf(stderr, "discarded packsts: %ld\n", dest_packet);
        return dest_packet;
    }
    size_t dest_all_packet() {
        pre_sequence_number = 0;
        recv_buf.clear();
        return recv_buf.dest(
            [](const uint8_t* const data) -> bool { return static_cast<bool>(data[J2KPayloadHeader_trait::get_MH(data + RTPHeader_trait::length)]); }
        );
    }

private:
    static constexpr size_t NUM_BUFFER      = 2;
    static constexpr size_t MAX_PACKET_SIZE = 1384;

    UDPReceiver udp;
    RTPHeader rtp_header;
    J2KPayloadHeader payload_header;

    uint8_t* pkt_data_ptr;
    size_t pkt_data_size;

    uint32_t pre_sequence_number;

    uint8_t* use_buf;
    // BufferPool<uint8_t, MAX_PACKET_SIZE, NUM_BUFFER> recv_buf;
    leaky_bucket_buf recv_buf;
};
