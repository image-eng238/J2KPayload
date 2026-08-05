#pragma once

#include <array>

#include "leaky_bucket_buf.hpp"
#include "opt_macro.hpp"
#include "wrap_number.hpp"
#include "packet_t.hpp"
#include "fixed_capacity_vector.hpp"

/*****************************************************************************************************************************************************************************/
/* RTPHeader                                                                                                                                                                 */
/*****************************************************************************************************************************************************************************/

namespace RTPHeader_trait {
    // geter
    inline constexpr uint8_t length = 12;
    inline constexpr uint8_t get_header_length() { return RTPHeader_trait::length; }
    inline constexpr uint8_t get_V(const uint8_t* const pointer) { return (pointer[0] & 0xC0) >> 6; }                                                    // version: 2 bits 0b10で固定
    inline constexpr uint8_t get_P(const uint8_t* const pointer) { return (pointer[0] & 0x20) >> 5; }                                                    // padding: 1 bit
    inline constexpr uint8_t get_X(const uint8_t* const pointer) { return (pointer[0] & 0x10) >> 4; }                                                    // extension: 1 bit
    inline constexpr uint8_t get_CC(const uint8_t* const pointer) { return pointer[0] & 0x0F; }                                                          // CSRC_count: 4 bits
    inline constexpr uint8_t get_M(const uint8_t* const pointer) { return (pointer[1] & 0x80) >> 7; }                                                    // marker: 1 bit
    inline constexpr uint8_t get_PT(const uint8_t* const pointer) { return pointer[1] & 0x7F; }                                                          // payload_type: 7 bits
    inline constexpr uint16_t get_sequence_number(const uint8_t* const pointer) { return pointer[2] << 8 | pointer[3]; }                                 // 16 bits
    inline constexpr uint32_t get_timestamp(const uint8_t* const pointer) { return pointer[4] << 24 | pointer[5] << 16 | pointer[6] << 8 | pointer[7]; } // 32 bits
    inline constexpr uint32_t get_SSRC(const uint8_t* const pointer) { return pointer[8] << 24 | pointer[9] << 16 | pointer[10] << 8 | pointer[11]; }    // 32 bits

    // seter
    inline constexpr void set_V(uint8_t* const pointer, uint8_t val) { pointer[0] &= (val << 6) | 0x3F; } // version: 2 bits 0b10で固定
    inline constexpr void set_P(uint8_t* const pointer, uint8_t val) { pointer[0] &= (val << 5) | 0xDF; } // padding: 1 bit
    inline constexpr void set_X(uint8_t* const pointer, uint8_t val) { pointer[0] &= (val << 4) | 0xEF; } // extension: 1 bit
    inline constexpr void set_CC(uint8_t* const pointer, uint8_t val) { pointer[0] &= val | 0xF0; }       // CSRC_count: 4 bits
    inline constexpr void set_M(uint8_t* const pointer, uint8_t val) { pointer[1] &= (val << 7) | 0x7F; } // marker: 1 bit
    inline constexpr void set_PT(uint8_t* const pointer, uint8_t val) { pointer[1] &= val | 0x80; }       // payload_type: 7 bits
    inline constexpr void set_sequence_number(uint8_t* const pointer, uint16_t val) {                     // 16 bits
        pointer[2] = static_cast<uint8_t>(val >> 8);
        pointer[3] = static_cast<uint8_t>(val);
    }
    inline constexpr void set_timestamp(uint8_t* const pointer, uint32_t val) { // 32 bits
        pointer[4] = static_cast<uint8_t>(val >> 24);
        pointer[5] = static_cast<uint8_t>(val >> 16);
        pointer[6] = static_cast<uint8_t>(val >> 8);
        pointer[7] = static_cast<uint8_t>(val);
    }
    inline constexpr void set_SSRC(uint8_t* const pointer, uint8_t val) { // 32 bits
        pointer[8]  = static_cast<uint8_t>(val >> 24);
        pointer[9]  = static_cast<uint8_t>(val >> 16);
        pointer[10] = static_cast<uint8_t>(val >> 8);
        pointer[11] = static_cast<uint8_t>(val);
    }

    inline void print_info(FILE* const fp, const uint8_t* const pointer) {
        fprintf(fp, "RTP packet header\n");
        fprintf(fp, "  V = %d\n", static_cast<uint32_t>(RTPHeader_trait::get_V(pointer)));
        fprintf(fp, "  P = %d\n", static_cast<uint32_t>(RTPHeader_trait::get_P(pointer)));
        fprintf(fp, "  X = %d\n", static_cast<uint32_t>(RTPHeader_trait::get_X(pointer)));
        fprintf(fp, "  CC = %d\n", static_cast<uint32_t>(RTPHeader_trait::get_CC(pointer)));
        fprintf(fp, "  M = %d\n", static_cast<uint32_t>(RTPHeader_trait::get_M(pointer)));
        fprintf(fp, "  PT = %d\n", static_cast<uint32_t>(RTPHeader_trait::get_PT(pointer)));
        fprintf(fp, "  sequence_number = %d\n", static_cast<uint32_t>(RTPHeader_trait::get_sequence_number(pointer)));
        fprintf(fp, "  timestamp = %d\n", static_cast<uint32_t>(RTPHeader_trait::get_timestamp(pointer)));
        fprintf(fp, "  SSRC = %d\n", static_cast<uint32_t>(RTPHeader_trait::get_SSRC(pointer)));
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

/*****************************************************************************************************************************************************************************/
/* J2KPayloadHeader                                                                                                                                                          */
/*****************************************************************************************************************************************************************************/

namespace J2KPayloadHeader_trait {
    constexpr uint8_t length           = 8;
    constexpr size_t media_clock_Hz    = 90'000;
    constexpr uint32_t ex_sequence_max = 0xFFFFFF;

    // geter
    inline constexpr uint8_t get_header_length() { return length; }
    inline constexpr uint8_t get_MH(const uint8_t* const pointer) { return (pointer[0] & 0xC0) >> 6; }                    // Codestream Main Header Presence: 2 bits
    inline constexpr uint8_t get_TP(const uint8_t* const pointer) { return (pointer[0] & 0x38) >> 3; }                    // Image Type: 3 bits
    inline constexpr uint16_t get_PTSTAMP(const uint8_t* const pointer) { return (pointer[1] & 0x0F) << 8 | pointer[2]; } // Precision Timestamp: 12 bits
    inline constexpr uint8_t get_ESEQ(const uint8_t* const pointer) { return pointer[3]; }                                // Extended Sequence Number High-Order Bits: 8 bits
    // main
    inline constexpr uint8_t get_main_ORDH(const uint8_t* const pointer) { return pointer[0] & 0x07; }         // Progression Order Flag, Main Packet: 3 bits
    inline constexpr uint8_t get_main_P(const uint8_t* const pointer) { return (pointer[1] & 0x80) >> 7; }     // Precision Timestamp Presence: 1 bit
    inline constexpr uint8_t get_main_XTRAC(const uint8_t* const pointer) { return (pointer[1] & 0x70) >> 4; } // Extension Payload Length: 3 bits
    inline constexpr uint16_t get_main_PTSTAMP(const uint8_t* const pointer) { return get_PTSTAMP(pointer); }  // Precision Timestamp: 12 bits
    inline constexpr uint8_t get_main_ESEQ(const uint8_t* const pointer) { return get_ESEQ(pointer); }         // Extended Sequence Number High-Order Bits: 8 bits

    inline constexpr uint8_t get_main_R(const uint8_t* const pointer) { return (pointer[4] & 0x80) >> 7; }    // Codestream Main Header Reuse: 1 bit
    inline constexpr uint8_t get_main_S(const uint8_t* const pointer) { return (pointer[4] & 0x40) >> 6; }    // Parameterized Colorspace Presence: 1 bit
    inline constexpr uint8_t get_main_C(const uint8_t* const pointer) { return (pointer[4] & 0x20) >> 5; }    // Code-Block Caching Usage: 1 bit
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

    // seter
    inline constexpr void set_MH(uint8_t* const pointer, uint8_t val) { pointer[0] &= (val << 6) | 0x3F; } // Codestream Main Header Presence: 2 bits
    inline constexpr void set_TP(uint8_t* const pointer, uint8_t val) { pointer[0] &= (val << 3) | 0xC7; } // Image Type: 3 bits
    inline constexpr void set_PTSTAMP(uint8_t* const pointer, uint16_t val) {                              // Precision Timestamp: 12 bits
        pointer[1] &= (val >> 8) | 0xF0;
        pointer[2] = val & 0xFF;
    }
    inline constexpr void set_ESEQ(uint8_t* const pointer, uint8_t val) { pointer[3] = val; } // Extended Sequence Number High-Order Bits: 8 bits
    // main
    inline constexpr void set_main_ORDH(uint8_t* const pointer, uint8_t val) { pointer[0] &= val | 0xF8; }                        // Progression Order Flag, Main Packet: 3 bits
    inline constexpr void set_main_P(uint8_t* const pointer, bool val) { pointer[1] &= (static_cast<uint8_t>(val) << 7) | 0x7F; } // Precision Timestamp Presence: 1 bit
    inline constexpr void set_main_XTRAC(uint8_t* const pointer, uint8_t val) { pointer[1] &= (val << 4) | 0x8F; }                // Extension Payload Length: 3 bits
    inline constexpr void set_main_PTSTAMP(uint8_t* const pointer, uint8_t val) { set_PTSTAMP(pointer, val); }                    // Precision Timestamp: 12 bits
    inline constexpr void set_main_ESEQ(uint8_t* const pointer, uint8_t val) { set_ESEQ(pointer, val); }                          // Extended Sequence Number High-Order Bits: 8 bits

    inline constexpr void set_main_R(uint8_t* const pointer, uint8_t val) { pointer[4] &= (val << 7) | 0x7F; }    // Codestream Main Header Reuse: 1 bit
    inline constexpr void set_main_S(uint8_t* const pointer, uint8_t val) { pointer[4] &= (val << 6) | 0xBF; }    // Parameterized Colorspace Presence: 1 bit
    inline constexpr void set_main_C(uint8_t* const pointer, uint8_t val) { pointer[4] &= (val << 5) | 0xDF; }    // Code-Block Caching Usage: 1 bit
    inline constexpr void set_main_RSVD(uint8_t* const pointer, uint8_t val) { pointer[4] &= (val << 1) | 0xE1; } // Reserved: 4 bits
    inline constexpr void set_main_RANGE(uint8_t* const pointer, uint8_t val) { pointer[4] &= val | 0xFE; }       // Video Full Range Usage: 1 bit
    inline constexpr void set_main_PRIMS(uint8_t* const pointer, uint8_t val) { pointer[5] = val; }               // Color Primaries: 8 bits
    inline constexpr void set_main_TRANS(uint8_t* const pointer, uint8_t val) { pointer[6] = val; }               // Transfer Characteristice: 8 bits
    inline constexpr void set_main_MAT(uint8_t* const pointer, uint8_t val) { pointer[7] = val; }                 // Colo Matrix Coefficients: 8 bits
    // body
    inline constexpr void set_body_RES(uint8_t* const pointer, uint8_t val) { pointer[0] &= val | 0xF8; }         // Resolution Levels: 3 bits
    inline constexpr void set_body_ORDB(uint8_t* const pointer, uint8_t val) { pointer[1] &= (val << 7) | 0x7F; } // Progression Order Flag, Body Packet: 1 bit is_resync_point
    inline constexpr void set_body_QUAL(uint8_t* const pointer, uint8_t val) { pointer[1] &= (val << 4) | 0x8F; } // Quality Layers: 3 bit
    inline constexpr void set_body_PTSTAMP(uint8_t* const pointer, uint8_t val) { set_PTSTAMP(pointer, val); }    // Precision Timestamp: 12 bits
    inline constexpr void set_body_ESEQ(uint8_t* const pointer, uint8_t val) { set_ESEQ(pointer, val); }          // Extended Sequence Number High-Order Bits: 8 bits

    inline constexpr void set_body_POS(uint8_t* const pointer, uint16_t val) { // Resyns Point Offset: 12 bits
        pointer[4] = static_cast<uint8_t>(val >> 4);
        pointer[5] &= (static_cast<uint8_t>(val & 0x4) << 4) | 0x0F;
    }
    inline constexpr void set_body_PID(uint8_t* const pointer, uint32_t val) { // Precinct Identifier: 20 bits
        pointer[5] &= static_cast<uint8_t>(val >> 16) | 0xF0;
        pointer[6] = static_cast<uint8_t>(val >> 8);
        pointer[7] = static_cast<uint8_t>(val);
    }

    inline constexpr void set_extended_sequence_number(uint8_t* const pointer, uint32_t val) {
        set_ESEQ(pointer + RTPHeader_trait::length, static_cast<uint8_t>(val >> 16));
        RTPHeader_trait::set_sequence_number(pointer, static_cast<uint16_t>(val));
    }

    inline void print_info(FILE* const fp, const uint8_t* const pointer) {
        const auto hd = RTPHeader_trait::get_header_length();
        fprintf(fp, "RTP payload header\n");
        fprintf(fp, "    MH = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_MH(pointer + hd)));
        fprintf(fp, "    TP = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_TP(pointer + hd)));
        if (J2KPayloadHeader_trait::get_MH(pointer + hd)) {
            fprintf(fp, "    ORDH = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_ORDH(pointer + hd)));
            fprintf(fp, "    P = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_P(pointer + hd)));
            fprintf(fp, "    XTRAC = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_XTRAC(pointer + hd)));
            fprintf(fp, "    PTSTAMP = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_PTSTAMP(pointer + hd)));
            fprintf(fp, "    ESEQ = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_ESEQ(pointer + hd)));
            fprintf(fp, "    R = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_R(pointer + hd)));
            fprintf(fp, "    S = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_S(pointer + hd)));
            fprintf(fp, "    C = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_C(pointer + hd)));
            fprintf(fp, "    RSVD = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_RSVD(pointer + hd)));
            fprintf(fp, "    RANGE = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_RANGE(pointer + hd)));
            fprintf(fp, "    PRIMS = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_PRIMS(pointer + hd)));
            fprintf(fp, "    TRANS = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_TRANS(pointer + hd)));
            fprintf(fp, "    MAT = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_main_MAT(pointer + hd)));
        } else {
            fprintf(fp, "    RES = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_body_RES(pointer + hd)));
            fprintf(fp, "    ORDB = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_body_ORDB(pointer + hd)));
            fprintf(fp, "    QUAL = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_body_QUAL(pointer + hd)));
            fprintf(fp, "    PTSTAMP = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_body_PTSTAMP(pointer + hd)));
            fprintf(fp, "    ESEQ = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_body_ESEQ(pointer + hd)));
            fprintf(fp, "    POS = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_body_POS(pointer + hd)));
            fprintf(fp, "    PID = %d\n", static_cast<uint32_t>(J2KPayloadHeader_trait::get_body_PID(pointer + hd)));
        };
        fprintf(fp, "extended_sequence_number = %d\n", J2KPayloadHeader_trait::get_extended_sequence_number(pointer));
    }
    inline void print_csv(FILE* const fp, const uint8_t* const pointer) {
        const auto hd      = RTPHeader_trait::get_header_length();
        const bool is_main = J2KPayloadHeader_trait::get_MH(pointer + hd);
        if (is_main) {
            fprintf(
                fp, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
                static_cast<int>(RTPHeader_trait::get_V(pointer)),
                static_cast<int>(RTPHeader_trait::get_P(pointer)),
                static_cast<int>(RTPHeader_trait::get_X(pointer)),
                static_cast<int>(RTPHeader_trait::get_CC(pointer)),
                static_cast<int>(RTPHeader_trait::get_M(pointer)),
                static_cast<int>(RTPHeader_trait::get_PT(pointer)),
                static_cast<int>(RTPHeader_trait::get_sequence_number(pointer)),
                static_cast<int>(RTPHeader_trait::get_timestamp(pointer)),
                static_cast<int>(RTPHeader_trait::get_SSRC(pointer)),
                static_cast<int>(J2KPayloadHeader_trait::get_MH(pointer + hd)),
                static_cast<int>(J2KPayloadHeader_trait::get_TP(pointer + hd)),
                static_cast<int>(J2KPayloadHeader_trait::get_PTSTAMP(pointer + hd)),
                static_cast<int>(J2KPayloadHeader_trait::get_ESEQ(pointer + hd)),
                -1,
                -1,
                -1,
                -1,
                -1,
                static_cast<int>(J2KPayloadHeader_trait::get_main_ORDH(pointer + hd)),
                static_cast<int>(J2KPayloadHeader_trait::get_main_P(pointer + hd)),
                static_cast<int>(J2KPayloadHeader_trait::get_main_XTRAC(pointer + hd)),
                static_cast<int>(J2KPayloadHeader_trait::get_main_R(pointer + hd)),
                static_cast<int>(J2KPayloadHeader_trait::get_main_S(pointer + hd)),
                static_cast<int>(J2KPayloadHeader_trait::get_main_C(pointer + hd)),
                static_cast<int>(J2KPayloadHeader_trait::get_main_RSVD(pointer + hd)),
                static_cast<int>(J2KPayloadHeader_trait::get_main_RANGE(pointer + hd)),
                static_cast<int>(J2KPayloadHeader_trait::get_main_PRIMS(pointer + hd)),
                static_cast<int>(J2KPayloadHeader_trait::get_main_TRANS(pointer + hd)),
                static_cast<int>(J2KPayloadHeader_trait::get_main_MAT(pointer + hd)),
                static_cast<int>(J2KPayloadHeader_trait::get_extended_sequence_number(pointer))
            );
        } else {
            fprintf(
                fp, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
                static_cast<int>(RTPHeader_trait::get_V(pointer)),
                static_cast<int>(RTPHeader_trait::get_P(pointer)),
                static_cast<int>(RTPHeader_trait::get_X(pointer)),
                static_cast<int>(RTPHeader_trait::get_CC(pointer)),
                static_cast<int>(RTPHeader_trait::get_M(pointer)),
                static_cast<int>(RTPHeader_trait::get_PT(pointer)),
                static_cast<int>(RTPHeader_trait::get_sequence_number(pointer)),
                static_cast<int>(RTPHeader_trait::get_timestamp(pointer)),
                static_cast<int>(RTPHeader_trait::get_SSRC(pointer)),
                static_cast<int>(J2KPayloadHeader_trait::get_MH(pointer + hd)),
                static_cast<int>(J2KPayloadHeader_trait::get_TP(pointer + hd)),
                static_cast<int>(J2KPayloadHeader_trait::get_PTSTAMP(pointer + hd)),
                static_cast<int>(J2KPayloadHeader_trait::get_ESEQ(pointer + hd)),
                static_cast<int>(J2KPayloadHeader_trait::get_body_RES(pointer + hd)),
                static_cast<int>(J2KPayloadHeader_trait::get_body_ORDB(pointer + hd)),
                static_cast<int>(J2KPayloadHeader_trait::get_body_QUAL(pointer + hd)),
                static_cast<int>(J2KPayloadHeader_trait::get_body_POS(pointer + hd)),
                static_cast<int>(J2KPayloadHeader_trait::get_body_PID(pointer + hd)),
                -1,
                -1,
                -1,
                -1,
                -1,
                -1,
                -1,
                -1,
                -1,
                -1,
                -1,
                static_cast<int>(J2KPayloadHeader_trait::get_extended_sequence_number(pointer))
            );
        }
        fflush(fp);
    }
    inline int print_csv_fmt(FILE* const fp, int seq = 1) {
        const size_t num_ccolumn = 30;
        fprintf(
            fp, "V[%d],P[%d],X[%d],CC[%d],M[%d],PT[%d],sequence_number[%d],timestamp[%d],SSRC[%d],MH[%d],TP[%d],PTSTAMP[%d],ESEQ[%d],RES[%d],ORDB[%d],QUAL[%d],POS[%d],PID[%d],ORDH[%d],P[%d],XTRAC[%d],R[%d],S[%d],C[%d],RSVD[%d],RANGE[%d],PRIMS[%d],TRANS[%d],MAT[%d],extended_sequence_number[%d]",
            0 + seq,
            1 + seq,
            2 + seq,
            3 + seq,
            4 + seq,
            5 + seq,
            6 + seq,
            7 + seq,
            8 + seq,
            9 + seq,
            10 + seq,
            11 + seq,
            12 + seq,
            13 + seq,
            14 + seq,
            15 + seq,
            16 + seq,
            17 + seq,
            18 + seq,
            19 + seq,
            20 + seq,
            21 + seq,
            22 + seq,
            23 + seq,
            24 + seq,
            25 + seq,
            26 + seq,
            27 + seq,
            28 + seq,
            29 + seq
        );
        fflush(fp);
        return 29 + seq;
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

using rtptimestamp_t = range_wrap_t<uint64_t, maximum_value_v<uint32_t>, 0>;
using exsequence_t   = range_wrap_t<uint32_t, J2KPayloadHeader_trait::ex_sequence_max, 0>;
using j2kptstamp_t   = range_wrap_t<uint16_t, 4095, 0>;

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
    enum {
        MAIN_PACKET,
        BODY_NO_RESYNC,
        BODY_RESYNC_HEAD,
        BODY_RESYNC_TAIL
    };

    int32_t first_check();
    int32_t check();
    void pop(uint8_t*&, size_t&);

    int load_main_packet();
    int load_body_packet();
    packet_t pop();
    void terminate() {
        pos    = 0;
        is_EOC = false;
        j2k_packets.clear();
    }

    uint32_t get_last_sequence_number() const { return pre_sequence_number; }
    void set_last_sequence_number(const uint32_t in) { pre_sequence_number = in; }
    uint32_t get_lost_packet() const { return num_lost_packet; }
    uint32_t get_PID() const { return PID; }
    bool EOC() const { return is_EOC; }
    static bool check_rtp_sequence(uint32_t prev, uint32_t current) {
        return (current == prev + 1) || (prev == 0 && current == 1) || (prev == J2KPayloadHeader_trait::ex_sequence_max || current == 0);
    }
    static packet_t parse_rtp_header(const packet_t&, int);

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
    fixed_capacity_vector<packet_t, 16> j2k_packets;
    uint8_t pos;
    uint8_t num_packets;
    bool is_EOC;
};
