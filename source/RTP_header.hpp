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

class RTPHeader {
public:
    RTPHeader() = default;
    RTPHeader(const uint8_t* const ptr) : pointer{ptr} {}

    void set_ptr(const uint8_t* const ptr) { pointer = ptr; }

    uint8_t get_V() const { return pointer[0] & 0xC0; };                                                           // version: 2 bits 0b10で固定
    uint8_t get_P() const { return pointer[0] & 0x20; };                                                           // padding: 1 bit
    uint8_t get_X() const { return pointer[0] & 0x10; };                                                           // extension: 1 bit
    uint8_t get_CC() const { return pointer[0] & 0x0F; };                                                          // CSRC_count: 4 bits
    uint8_t get_M() const { return pointer[1] & 0x80; };                                                           // marker: 1 bit
    uint8_t get_PT() const { return pointer[1] & 0x7F; };                                                          // payload_type: 7 bits
    uint16_t get_sequence_number() const { return pointer[2] << 8 | pointer[3]; };                                 // 16 bits
    uint32_t get_timestamp() const { return pointer[4] << 24 | pointer[5] << 16 | pointer[6] << 8 | pointer[7]; }; // 32 bits
    uint32_t get_SSRC() const { return pointer[8] << 24 | pointer[9] << 16 | pointer[10] << 8 | pointer[11]; };    // 32 bits
    static constexpr uint8_t get_header_length() { return length; }

private:
    const uint8_t* pointer;
    static constexpr uint8_t length = 12;
};

class J2KPayloadHeader {
public:
    J2KPayloadHeader() = default;
    J2KPayloadHeader(const uint8_t* const ptr) : pointer{ptr} {}

    void set_ptr(const uint8_t* const ptr) { pointer = ptr; }

    uint8_t get_MH() const { return pointer[0] & 0xC0; }                           // Codestream Main Header Presence: 2 bits
    uint8_t get_TP() const { return pointer[0] & 0x38; }                           // Image Type: 3 bits
    uint16_t get_PTSTAMP() const { return (pointer[1] & 0x0F) << 8 | pointer[2]; } // Precision Timestamp: 12 bits
    uint8_t get_ESEQ() const { return pointer[3]; }                                // Extended Sequence Number High-Order Bits: 8 bits
    static constexpr uint8_t get_header_length() { return length; }

    // main
    uint8_t get_main_ORDH() const { return pointer[0] & 0x07; }  // Progression Order Flag, Main Packet: 3 bits
    uint8_t get_main_P() const { return pointer[1] & 0x80; }     // Precision Timestamp Presence: 1 bit
    uint8_t get_main_XTRAC() const { return pointer[1] & 0x70; } // Extension Payload Length: 3 bits
    uint16_t get_main_PTSTAMP() const { return get_PTSTAMP(); }  // Precision Timestamp: 12 bits
    uint8_t get_main_ESEQ() const { return get_ESEQ(); }         // Extended Sequence Number High-Order Bits: 8 bits

    uint8_t get_main_R() const { return pointer[4] & 0x80; }     // Codestream Main Header Reuse: 1 bit
    uint8_t get_main_S() const { return pointer[4] & 0x40; }     // Parameterized Colorspace Presence: 1 bit
    uint8_t get_main_C() const { return pointer[4] & 0x20; }     // Code-Block Caching Usage: 1 bit
    uint8_t get_main_RSVD() const { return pointer[4] & 0x1E; }  // Reserved: 4 bits
    uint8_t get_main_RANGE() const { return pointer[4] & 0x01; } // Video Full Range Usage: 1 bit
    uint8_t get_main_PRIMS() const { return pointer[5]; }        // Color Primaries: 8 bits
    uint8_t get_main_TRANS() const { return pointer[6]; }        // Transfer Characteristice: 8 bits
    uint8_t get_main_MAT() const { return pointer[7]; }          // Colo Matrix Coefficients: 8 bits

    // body
    uint8_t get_body_RES() const { return pointer[0] & 0x07; }  // Resolution Levels: 3 bits
    uint8_t get_body_ORDB() const { return pointer[1] & 0x80; } // Progression Order Flag, Body Packet: 1 bit is_resync_point
    uint8_t get_body_QUAL() const { return pointer[1] & 0x70; } // Quality Layers: 3 bit
    uint16_t get_body_PTSTAMP() const { return get_PTSTAMP(); } // Precision Timestamp: 12 bits
    uint8_t get_body_ESEQ() const { return get_ESEQ(); }        // Extended Sequence Number High-Order Bits: 8 bits

    uint16_t get_body_POS() const { return (pointer[4] << 4) | ((pointer[5] & 0xF0) >> 4); }               // Resyns Point Offset: 12 bits
    uint32_t get_body_PID() const { return (pointer[5] & 0x0F << 16) | (pointer[6] << 8) | (pointer[7]); } // Precinct Identifier: 20 bits

private:
    const uint8_t* pointer;
    static constexpr uint8_t length = 8;
};

class RTPReceiver {
public:
    RTPReceiver() : udp{}, rtp_header{}, payload_header{}, pre_sequence_number{}, use_buf{}, recv_buf{&udp} {};
    RTPReceiver(const char* const address, const uint16_t port)
        : udp{address, port}, rtp_header{}, payload_header{}, pre_sequence_number{}, use_buf{}, recv_buf{&udp} {}

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

    // bool receive() {

    //     auto pkt_size = recv_buf.pop(use_buf);

    //     if (unlikely(!(use_buf[0] & 0x80))) { return false; }

    //     this->rtp_header.set_ptr(use_buf);
    //     this->payload_header.set_ptr(use_buf + rtp_header.get_header_length());

    //     uint32_t extended_sequence_number = get_extended_sequence_number();

    //     if (likely((extended_sequence_number == pre_sequence_number + 1) || (pre_sequence_number == 0) || (extended_sequence_number == 0))) {
    //         pkt_data_ptr        = use_buf + rtp_header.get_header_length() + payload_header.get_header_length();
    //         pkt_data_size       = pkt_size - (rtp_header.get_header_length() + payload_header.get_header_length());
    //         pre_sequence_number = extended_sequence_number;

    //         return true;
    //     } else {
    //         // fprintf(stderr, "RTP sequence error, pre_seq: %d, seq: %d, lost packets: %d, ", pre_sequence_number, extended_sequence_number, extended_sequence_number - (pre_sequence_number + 1));
    //         rtp_sequence_error err;
    //         err.pre_sq          = pre_sequence_number;
    //         err.err_sq          = extended_sequence_number;
    //         pkt_data_ptr        = nullptr;
    //         pkt_data_size       = 0;
    //         pre_sequence_number = extended_sequence_number;

    //         // std::this_thread::yield();
    //         throw err;
    //     }
    // }
    // size_t dest_packt(const size_t& n) {
    //     // auto dest_packet    = recv_buf.dest([](const uint8_t* const data) { return static_cast<bool>(data[RTPHeader::get_header_length()] & 0xC0); });
    //     // pre_sequence_number = 0;
    //     auto dest_packet = recv_buf.get_accesser(n)->dest(
    //         [](const uint8_t* const data) -> bool { return static_cast<bool>(data[RTPHeader::get_header_length()] & 0xC0); },
    //         [this](const uint8_t* const data) -> void { this->pre_sequence_number = RTPReceiver::get_extended_sequence_number(data); }
    //     );
    //     // fprintf(stderr, "discarded packsts: %ld\n", dest_packet);
    //     return dest_packet;
    // }

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
    leaky_bucket_buf recv_buf;
};

class RTPReceiver_ref {
public:
    RTPReceiver_ref() : rtp_header{}, payload_header{}, pre_sequence_number{}, image_timestamp{}, recv_buf{} {};
    RTPReceiver_ref(lbb_access* const ptr)
        : rtp_header{}, payload_header{}, pre_sequence_number{}, image_timestamp{}, recv_buf{ptr} {};

    RTPHeader& access_rtp() { return rtp_header; }
    J2KPayloadHeader& access_payload() { return payload_header; }
    uint8_t*& access_pkt_data_ptr() { return pkt_data_ptr; }
    size_t& access_pkt_data_size() { return pkt_data_size; }
    auto get_recv_buf_ptr() { return recv_buf; }

    uint32_t get_extended_sequence_number() const { return (payload_header.get_ESEQ() << 16) | rtp_header.get_sequence_number(); }
    static uint32_t get_extended_sequence_number(const uint8_t* const data) { return static_cast<uint32_t>(data[15] << 0x10) | static_cast<uint32_t>(data[2] << 0x8) | static_cast<uint32_t>(data[3]); }

    bool pop() {
        uint8_t* tmp_buf;
        auto pkt_size = recv_buf->pop(tmp_buf);

        if (unlikely(!(tmp_buf[0] & 0x80))) { return false; }

        this->rtp_header.set_ptr(tmp_buf);
        this->payload_header.set_ptr(tmp_buf + rtp_header.get_header_length());

        uint32_t extended_sequence_number = get_extended_sequence_number();

        if (likely((extended_sequence_number == pre_sequence_number + 1) || (pre_sequence_number == 0) || (extended_sequence_number == 0))) {
            pkt_data_ptr        = tmp_buf + rtp_header.get_header_length() + payload_header.get_header_length();
            pkt_data_size       = pkt_size - (rtp_header.get_header_length() + payload_header.get_header_length());
            pre_sequence_number = extended_sequence_number;

            return true;
        } else if (image_timestamp != rtp_header.get_timestamp() || image_timestamp == 0) {
            pkt_data_ptr        = tmp_buf + rtp_header.get_header_length() + payload_header.get_header_length();
            pkt_data_size       = pkt_size - (rtp_header.get_header_length() + payload_header.get_header_length());
            pre_sequence_number = extended_sequence_number;
            image_timestamp     = rtp_header.get_timestamp();

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

    size_t dest_packt() {
        auto dest_packet = recv_buf->dest(
            [](const uint8_t* const data) -> bool { return static_cast<bool>(data[RTPHeader::get_header_length()] & 0xC0); }
        );
        pre_sequence_number = 0;
        image_timestamp     = 0;
        return dest_packet;
    }

private:
    RTPHeader rtp_header;
    J2KPayloadHeader payload_header;

    uint8_t* pkt_data_ptr;
    size_t pkt_data_size;
    uint32_t pre_sequence_number;
    uint32_t image_timestamp;

    lbb_access* recv_buf;
};