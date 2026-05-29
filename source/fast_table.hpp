#pragma once

#include "codestream.hpp"
#include "decoding.hpp"

class CodeBlock;
class Precinct;

struct fast_table {
    uint8_t number_of_subband;
    uint8_t deb_resolution_level;
    uint8_t deb_band_pos[3];
    CodeBlock ps_codeblock[3];
    uint32_t PID;

    void set(const Precinct* const p, const uint32_t pid) {
        number_of_subband    = p->get_number_of_subband();
        deb_resolution_level = p->get_resolution_level();
        PID                  = pid;
        for (uint8_t i = 0; i < number_of_subband; ++i) {
            deb_band_pos[i] = p->get_psubband_ptr(i)->get_band_pos();
            ps_codeblock[i].copy_pos(*p->get_psubband_ptr(i)->get_codeblock_ptr(0));
        }
    }

    void read_packet(J2kBuf& payload_buf) {
        static size_t call_count = 0;
        call_count++;
        if (unlikely(!payload_buf.get_bit())) { // empty packet
            throw J2K_packet_error(J2K_packet_error::empty_packet);
        }
        for (uint8_t i = 0; i < this->number_of_subband; ++i) {
#ifdef GENERATE_LOG
            ps_codeblock[i].read_packet_header(&payload_buf, deb_band_pos[i], deb_resolution_level);
#else
            ps_codeblock[i].read_packet_header(&payload_buf);
#endif
        }
        payload_buf.check_FF();
        payload_buf.r_fill();

        for (uint8_t i = 0; i < number_of_subband; ++i) {
            ps_codeblock[i].set_data(&payload_buf);
        }
        return;
    }
};