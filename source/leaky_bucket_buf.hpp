#pragma once

#include "UDP.hpp"

#include <cstdint>
#include <cstddef>
#include <cassert>

#include <mutex>
#include <condition_variable>

#include <atomic>

class leaky_bucket_buf;
struct ring_element {
    static constexpr size_t PACKET_SIZE = 1384;
    int data_size;
    uint8_t data[PACKET_SIZE];
    ring_element* next_ptr;
    bool empty() const { return data_size <= 0; }
};

class lbb_access {
    friend leaky_bucket_buf;

public:
    int pop(uint8_t*&);
    size_t get_num_data_unsafe() const { return num_data; }

private:
    ring_element* next_pop;
    size_t num_data;

    std::mutex mtx;
    std::condition_variable cond;

public:
    // 条件式 pred(uint8_t* data) を満たすまでパケットを捨てる data はパケットデータの先頭のポインタ
    template <typename Predcate>
    size_t dest(Predcate pred) {
        std::unique_lock lk(mtx, std::defer_lock);
        // size_t dest_packet = 0;
        size_t num_dest = 0;
        size_t up_limit = 0;
        while (true) {
            lk.lock();
            cond.wait(lk, [this] { return num_data > 0; });
            // const size_t up_limit = current_num_data;
            up_limit = num_data;
            lk.unlock();
            // size_t num_dest = 0;
            // pred が true になるか，データがなくなるまでパケットを破棄
            // アクセスできるデータがなくなったら，アクセスできるデータ数を更新
            while (num_dest < up_limit) {
                ring_element* const popping = next_pop;
                ++num_dest;
                popping->data_size = 0;
                next_pop           = popping->next_ptr;
                if (pred(popping->data)) {
                    lk.lock();
                    num_data -= num_dest;
                    lk.unlock();
                    // return dest_packet;
                    return num_dest;
                }
            }
            // lk.lock();
            // current_num_data -= num_dest;
            // lk.unlock();
            // dest_packet += num_dest;
        }
    }
    // 条件式 pred(uint8_t* data) を満たすまでパケットを捨てる data はパケットデータの先頭のポインタ
    // callback(uint8_t* data) で直前に破棄したバッファから必要なデータを抜きだす
    template <typename Predcate, typename Callback>
    size_t dest(Predcate pred, Callback callback) {
        std::unique_lock lk(mtx, std::defer_lock);
        // size_t dest_packet = 0;
        size_t num_dest = 0;
        size_t up_limit = 0;
        while (true) {
            lk.lock();
            cond.wait(lk, [this] { return num_data > 0; });
            // const size_t up_limit = current_num_data;
            up_limit = num_data;
            lk.unlock();
            // size_t num_dest = 0;
            // pred が true になるか，データがなくなるまでパケットを破棄
            // アクセスできるデータがなくなったら，アクセスできるデータ数を更新
            while (num_dest < up_limit) {
                ring_element* const popping = next_pop;
                ++num_dest;
                popping->data_size = 0;
                next_pop           = popping->next_ptr;
                callback(popping->data);
                if (pred(popping->data)) {
                    lk.lock();
                    num_data -= num_dest;
                    lk.unlock();
                    // return dest_packet;
                    return num_dest;
                }
            }
            // lk.lock();
            // current_num_data -= num_dest;
            // lk.unlock();
            // dest_packet += num_dest;
        }
    }
};

class leaky_bucket_buf {
public:
#ifdef GENERATE_RECEIVE_PROBABILITY
    inline static size_t count_receive = 0;
    inline static size_t count_agaein  = 0;
#endif
    static constexpr size_t BUFFER_LENGTH = 3000;
    static constexpr size_t NUM_BUFFER    = 2;
    leaky_bucket_buf();
    leaky_bucket_buf(UDPReceiver* const);

    constexpr void set_udp(UDPReceiver* const);
    bool receive();
    // int pop(uint8_t*&);
    static uint32_t get_seq(const uint8_t* const data) { return static_cast<uint32_t>(data[15] << 0x10) | static_cast<uint32_t>(data[2] << 0x8) | static_cast<uint32_t>(data[3]); }

    size_t get_num_data_unsafe() const { return current_buffer->num_data + tmp_num_data; }

    lbb_access* get_accesser(const size_t& n) { return &accesser[n]; }

private:
    ring_element* next_write; // receive からのみアクセス
    UDPReceiver* udp;         // receive のみからアクセス

    size_t tmp_num_data;

    lbb_access* current_buffer;
    size_t current_buffer_pos;

    ring_element buf_list[NUM_BUFFER][BUFFER_LENGTH];
    lbb_access accesser[NUM_BUFFER];
};
