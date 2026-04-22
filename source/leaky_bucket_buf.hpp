#pragma once

#include "UDP.hpp"

#include <cstdint>
#include <cstddef>

#include <mutex>
#include <condition_variable>

#include <atomic>

class leaky_bucket_buf {

public:
    static constexpr size_t BUFFER_SIZE = 1384;
    static constexpr size_t NUM_BUFFER  = 5000;

    leaky_bucket_buf();
    leaky_bucket_buf(UDPReceiver* const);

    constexpr void set_udp(UDPReceiver* const);
    bool receive();
    void write(const uint8_t* const, const size_t&);
    uint8_t* pop();
    int pop(uint8_t*&);
    static uint32_t get_seq(const uint8_t* const data) { return (data[0] & 0x80) ? static_cast<uint32_t>(data[15] << 0x10) | static_cast<uint32_t>(data[2] << 0x8) | static_cast<uint32_t>(data[3]) : 0; }

private:
    struct link_list {
        link_list* next_ptr;
        int data_size;
        uint8_t data[leaky_bucket_buf::BUFFER_SIZE];
        bool empty() const { return data_size <= 0; }
    };
    link_list* next_write; // receive からのみアクセス
    link_list* next_pop;   // pop からのみアクセス
    UDPReceiver* udp;      // receive のみからアクセス

    std::atomic_size_t current_num_data;

    link_list buf_list[NUM_BUFFER];
    // 体感では link_list のメンバに配列をもたせたほうが NUM_BUFFER が小さい値でも安定する(要検証)
};
