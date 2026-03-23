#include "codestream.hpp"
#include <cassert>

void J2kBuf::step(const int64_t& in) { buf_ptr += in; }
void J2kBuf::r_fill() {
    if (!(bit_pos & 0x80)) {
        bit_pos = 128;
        buf_ptr++;
    }
}
void J2kBuf::l_fill() {
    bit_pos = 128;
}
void J2kBuf::reset(uint8_t* const in) {
    buf_ptr = in;
    bit_pos = 128;
}

uint8_t J2kBuf::get_bit() {
    const uint8_t out = *buf_ptr & bit_pos;
    if (bit_pos & 1) {
        bit_pos = 128;
        if (*buf_ptr == 0xFF) { // bit stuffing
            bit_pos >>= 1;
        }
        buf_ptr++;
    } else {
        bit_pos >>= 1;
    }
    return (out) ? 1 : 0;
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
    bit_pos = 128;
    return *buf_ptr++;
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
    if (buf_ptr[-1] == 0xFF) {
        if (bit_pos & 1) {
            bit_pos = 128;
            buf_ptr++;
        } else {
            bit_pos >>= 1;
        }
    }
}

uint8_t* J2kBuf::get_ptr() const { return buf_ptr; }