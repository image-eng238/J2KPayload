#include "codestream.hpp"
#include <cassert>
#include <cstring>

void J2kBuf::step(const int64_t& in) {
    advance_byte_pos(in);
    if (recv->access_payload().get_MH() == 0) termination_check();
}
void J2kBuf::r_fill() {
    if (!(bit_pos & 0x80)) {
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
    if (bit_pos & 0x80) termination_check();
    const uint8_t out = buf_ptr[byte_pos] & bit_pos;
    if (bit_pos & 1) {
        bit_pos = 0x80;
        if (buf_ptr[byte_pos] == 0xFF) { // bit stuffing
            bit_pos >>= 1;
        }
        advance_byte_pos(1);
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
    if (buf_ptr[byte_pos - 1] == 0xFF) {
        if (bit_pos & 1) {
            bit_pos = 0x80;
            advance_byte_pos(1);
        } else {
            bit_pos >>= 1;
        }
    }
}

uint8_t* J2kBuf::get_ptr() const { return &buf_ptr[byte_pos]; }

uint8_t* J2kBuf::make_packet_data(const size_t& len, uint8_t* const ptr) {
    r_fill();
    if (byte_pos + len < buf_length) {
        memcpy(ptr, get_ptr(), len);
        byte_pos += len;
        return ptr;
    } else {
        size_t cp1len = buf_length - byte_pos;
        memcpy(ptr, get_ptr(), cp1len);
        receive();
        size_t cp2len = len - cp1len;
        memcpy(ptr + cp1len, get_ptr(), cp2len);
        byte_pos += cp2len;
        return ptr;
    }
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