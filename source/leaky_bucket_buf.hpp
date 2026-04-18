#pragma once

#include "UDP.hpp"

#include <cstdint>
#include <cstddef>

#include <mutex>
#include <condition_variable>
class leaky_bucket_buf {

public:
    static constexpr size_t BUFFER_SIZE = 1384;
    static constexpr size_t NUM_BUFFER  = 150000;

    leaky_bucket_buf();
    leaky_bucket_buf(UDPReceiver* const);

    constexpr void set_udp(UDPReceiver* const);
    void receive();
    void write(const uint8_t* const, const size_t&);
    uint8_t* pop();
    int pop(uint8_t*&);

private:
    struct link_list {
        link_list* next_ptr;
        int data_size;
        uint8_t data[leaky_bucket_buf::BUFFER_SIZE];
        // uint8_t* data;
        bool empty() const { return data_size == 0; }
    };
    link_list* next_write;
    link_list* next_pop;
    UDPReceiver* udp;

    std::mutex mtx;
    std::condition_variable can_pop;
    std::condition_variable can_receive;
    size_t current_num_data;

    link_list buf_list[NUM_BUFFER];
    // uint8_t buffer[BUFFER_SIZE * NUM_BUFFER];
    // 体感では link_list のメンバに配列をもたせたほうが NUM_BUFFER が小さい値でも安定する(要検証)
};
