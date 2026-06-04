#pragma once

#include <array>
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

    inline void print_info(const uint8_t* const pointer) {
        printf("RTP packet header\n");
        printf("  V = %d\n", static_cast<uint32_t>(RTPHeader_trait::get_V(pointer)));
        printf("  P = %d\n", static_cast<uint32_t>(RTPHeader_trait::get_P(pointer)));
        printf("  X = %d\n", static_cast<uint32_t>(RTPHeader_trait::get_X(pointer)));
        printf("  CC = %d\n", static_cast<uint32_t>(RTPHeader_trait::get_CC(pointer)));
        printf("  M = %d\n", static_cast<uint32_t>(RTPHeader_trait::get_M(pointer)));
        printf("  PT = %d\n", static_cast<uint32_t>(RTPHeader_trait::get_PT(pointer)));
        printf("  sequence_number = %d\n", static_cast<uint32_t>(RTPHeader_trait::get_sequence_number(pointer)));
        printf("  timestamp = %d\n", static_cast<uint32_t>(RTPHeader_trait::get_timestamp(pointer)));
        printf("  SSRC = %d\n", static_cast<uint32_t>(RTPHeader_trait::get_SSRC(pointer)));
    }
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

    inline void print_info(const uint8_t* const pointer) {
        const auto hd = RTPHeader_trait::get_header_length();
        printf("RTP payload header\n");
        printf("    MH = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_MH(pointer + hd)));
        printf("    TP = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_TP(pointer + hd)));
        if (J2KPayloadHeader_trait::get_MH(pointer + hd)) {
            printf("    ORDH = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_ORDH(pointer + hd)));
            printf("    P = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_P(pointer + hd)));
            printf("    XTRAC = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_XTRAC(pointer + hd)));
            printf("    PTSTAMP = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_PTSTAMP(pointer + hd)));
            printf("    ESEQ = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_ESEQ(pointer + hd)));
            printf("    R = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_R(pointer + hd)));
            printf("    S = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_S(pointer + hd)));
            printf("    C = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_C(pointer + hd)));
            printf("    RSVD = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_RSVD(pointer + hd)));
            printf("    RANGE = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_RANGE(pointer + hd)));
            printf("    PRIMS = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_PRIMS(pointer + hd)));
            printf("    TRANS = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_TRANS(pointer + hd)));
            printf("    MAT = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_MAT(pointer + hd)));
        } else {
            printf("    RES = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_body_RES(pointer + hd)));
            printf("    ORDB = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_body_ORDB(pointer + hd)));
            printf("    QUAL = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_body_QUAL(pointer + hd)));
            printf("    PTSTAMP = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_body_PTSTAMP(pointer + hd)));
            printf("    ESEQ = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_body_ESEQ(pointer + hd)));
            printf("    POS = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_body_POS(pointer + hd)));
            printf("    PID = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_body_PID(pointer + hd)));
        };
        printf("extended_sequence_number = %d\n", J2KPayloadHeader_trait::get_extended_sequence_number(pointer));
    }

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
    RTPReceiver(leaky_bucket_buf* const ptr)
        : buffer{ptr}, pre_sequence_number{}, num_lost_packet{}, PID{}, cache{}, packets{}, pos{}, num_packets{1}, is_EOC{false} {}
    enum {
        FAILURE     = 0,
        SUCCESS     = 1,
        MAIN_HEADER = 2,
        FINISH      = 3,
    };

    int32_t check() {
        using namespace RTPHeader_trait;
        using namespace J2KPayloadHeader_trait;
        const auto hl = RTPHeader_trait::get_header_length();

        cache = packets[num_packets - 1];

        pos          = 0;
        num_packets  = 0;
        uint8_t rpos = 0;
        if (unlikely(is_EOC)) is_EOC = false;

        while (true) {
            auto& data = packets[rpos].data;
            auto& len  = packets[rpos].len;
            ++rpos;
            len = buffer->pop(data);
            if (unlikely(len == 1 && RTPHeader_trait::get_V(data) != 0b10)) return this->FINISH;
            ++num_packets;

            const auto sequence     = get_extended_sequence_number(data);
            const auto pre_sequence = pre_sequence_number;
            pre_sequence_number     = sequence;

            if (likely(sequence == pre_sequence + 1 || pre_sequence == 0 || sequence == 0)) {
                if (unlikely(get_MH(data + hl))) { // メインヘッダ出現
                    assert(num_packets == 1);
                    cache = {};
                    return this->MAIN_HEADER;
                }

                if (unlikely(get_M(data))) { is_EOC = true; } // コードストリーム終端

                if (get_body_ORDB(data + hl)) { // 再同期ポイントが出現した場合 J2K パケットの解析が可能に
                    PID = get_body_PID(data + hl);
                    return this->SUCCESS;
                }
            } else {
                // パケットロス発生 次の再同期ポイントまでパケットを破棄
                num_lost_packet = sequence - (pre_sequence + 1);
                while (true) {
                    len = buffer->pop(data);
                    if (!get_MH(data + hl) && get_body_ORDB(data + hl)) {
                        pre_sequence_number = get_extended_sequence_number(data);
                        PID                 = get_body_PID(data + hl);
                        return this->FAILURE;
                    }
                }
            }
        }
    }

    void pop(uint8_t*& ptr, size_t& len) {
        const auto hl = RTPHeader_trait::get_header_length() + J2KPayloadHeader_trait::get_header_length();

        if (cache.empty() || cache.is_main()) {
            if (unlikely(!(pos < num_packets))) { throw buffer_leak("out range"); }
            const auto& pkt = packets[pos++];
            ptr             = pkt.data + hl;
            if (pos != num_packets) {
                assert(!J2KPayloadHeader_trait::get_body_ORDB(pkt.data + RTPHeader_trait::get_header_length()));
                len = static_cast<size_t>(pkt.len - hl);
            } else {
                assert(J2KPayloadHeader_trait::get_body_ORDB(pkt.data + RTPHeader_trait::get_header_length()));
                len = J2KPayloadHeader_trait::get_body_POS(pkt.data + RTPHeader_trait::get_header_length());
            }
        } else {
            ptr   = cache.data + hl + J2KPayloadHeader_trait::get_body_POS(cache.data + RTPHeader_trait::get_header_length());
            len   = static_cast<size_t>(cache.len - hl - J2KPayloadHeader_trait::get_body_POS(cache.data + RTPHeader_trait::get_header_length()));
            cache = {};
        }
    }

    uint32_t get_last_sequence_number() const { return pre_sequence_number; }
    uint32_t get_lost_packet() const { return num_lost_packet; }
    uint32_t get_PID() const { return PID; }
    bool EOC() const { return is_EOC; }

private:
    leaky_bucket_buf* buffer;
    uint32_t pre_sequence_number;
    uint32_t num_lost_packet;
    uint32_t PID;

    struct packet {
        uint8_t* data;
        int len;
        bool empty() const { return data == nullptr && len == 0; }
        bool is_main() const { return J2KPayloadHeader_trait::get_MH(data + RTPHeader_trait::get_header_length()); }
    };
    packet cache;
    std::array<packet, 16> packets;
    uint8_t pos;
    uint8_t num_packets;
    bool is_EOC;
};
