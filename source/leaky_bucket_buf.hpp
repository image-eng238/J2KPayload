#pragma once

#include "UDP.hpp"

#include <cstdint>
#include <cstddef>

#include <mutex>
#include <condition_variable>
class leaky_bucket_buf {

public:
    static constexpr size_t BUFFER_SIZE = 1500;
    static constexpr size_t NUM_BUFFER  = 2000;

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
};
