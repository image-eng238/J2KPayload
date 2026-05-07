#include "codestream.hpp"
#include <cassert>
#include <cstring>

#include "opt_macro.hpp"

void J2kBuf::step(const int64_t& in) {
    advance_byte_pos(in);
    if (recv->access_payload().get_MH() == 0) termination_check();
}
void J2kBuf::r_fill() {
    if (likely_p(!(bit_pos & 0x80), 0.875)) {
        bit_pos = 0x80;
        advance_byte_pos(1);
    }
}
void J2kBuf::l_fill() {
    if (!(bit_pos & 0x80)) {
        bit_pos = 0x80;
    }
}
void J2kBuf::reset(uint8_t* const in) {
    buf_ptr  = in;
    bit_pos  = 0x80;
    byte_pos = 0;
}

uint8_t J2kBuf::get_bit() {
    if (unlikely_p(bit_pos & static_cast<uint8_t>(0x80), 0.875)) {
        termination_check();
        if (unlikely(bit_purge == static_cast<uint8_t>(0xFF))) { // bit stuffing
            bit_pos >>= 1;
        }
        bit_purge = buf_ptr[byte_pos];
    }
    // const uint8_t out = buf_ptr[byte_pos] & bit_pos;
    const uint8_t out = bit_purge & bit_pos;
    if (unlikely_p(bit_pos & static_cast<uint8_t>(0x01), 0.875)) {
        bit_pos = static_cast<uint8_t>(0x80);
        advance_byte_pos(1);
    } else {
        bit_pos >>= 1;
    }
    // return (out) ? 1 : 0;
    return static_cast<uint8_t>(static_cast<bool>(out));
}
uint32_t J2kBuf::get_bit(const uint8_t& n) {
    assert(n <= 32);
    uint32_t out = 0;
    for (uint8_t i = 0; i < n; ++i) {
        out = (out << 1) | get_bit();
    }
    return out;
}

uint8_t J2kBuf::get_byte() {
    if (bit_pos | 0x7F) {
        bit_pos = 0x80;
    }
    termination_check();
    uint8_t out = buf_ptr[byte_pos];
    advance_byte_pos(1);
    return out;
}
uint32_t J2kBuf::get_byte(const uint8_t& n) {
    assert(n <= 16);
    uint32_t out = 0;
    for (uint8_t i = 0; i < n; ++i) {
        out = (out << 8) | get_byte();
    }
    return out;
}

void J2kBuf::check_FF() {
    if (bit_purge == 0xFF) {
        get_bit();
    }
}

uint8_t* J2kBuf::get_ptr() const { return &buf_ptr[byte_pos]; }

uint8_t* J2kBuf::make_packet_data(const size_t& len, uint8_t* const ptr) {
    static size_t call_count = 0;
    ++call_count;
    r_fill();
    if (byte_pos + len < buf_length) {
        memcpy(ptr, get_ptr(), len);
        byte_pos += len;
    } else {
        uint8_t* dest_ptr      = ptr;
        const uint8_t* src_ptr = get_ptr();
        size_t cpd             = len;
        size_t cplen           = buf_length - byte_pos;
        memcpy(dest_ptr, src_ptr, cplen);
        receive();

        for (size_t i = 0; i < 5; ++i) {
            dest_ptr += cplen;
            src_ptr = get_ptr();
            cpd -= cplen;
            cplen = std::min(cpd, buf_length);
            memcpy(dest_ptr, src_ptr, cplen);
            if (cplen == buf_length) {
                receive();
            } else {
                break;
            }
        }
        byte_pos += cplen;
    }
    return ptr;
}

void J2kBuf::advance_byte_pos(const size_t& n) {
    // if (byte_pos + n <= buf_length) {
    //     byte_pos += n;
    // } else {
    //     assert(false);
    // }
    byte_pos += n;
}

void J2kBuf::termination_check() {
    if (byte_pos < buf_length) {
        ;
    } else {
        receive();
    }
}
void J2kBuf::termination_check(const size_t& n) {
    if (byte_pos + n < buf_length) {
        ;
    } else {
        assert(false);
    }
}
void J2kBuf::receive() {
    assert(recv != nullptr);
    if (recv->receive()) {
        byte_pos   = 0;
        bit_pos    = 0x80;
        buf_ptr    = recv->access_pkt_data_ptr();
        buf_length = recv->access_pkt_data_size();
    }
}