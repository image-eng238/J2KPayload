#pragma once

#include "UDP.hpp"

#include <cstdint>
#include <cstddef>
#include <cassert>

#include <mutex>
#include <condition_variable>

#include <atomic>

class leaky_bucket_buf {

public:
    static constexpr size_t BUFFER_SIZE = 1384;
    static constexpr size_t NUM_BUFFER  = 2000;

    leaky_bucket_buf();
    leaky_bucket_buf(UDPReceiver* const);

    constexpr void set_udp(UDPReceiver* const);
    bool receive();
    int pop(uint8_t*&);
    static uint32_t get_seq(const uint8_t* const data) { return static_cast<uint32_t>(data[15] << 0x10) | static_cast<uint32_t>(data[2] << 0x8) | static_cast<uint32_t>(data[3]); }

private:
    struct link_list {
        uint8_t data[leaky_bucket_buf::BUFFER_SIZE];
        link_list* next_ptr;
        int data_size;
        bool empty() const { return data_size <= 0; }
    };
    link_list* next_write; // receive からのみアクセス
    link_list* next_pop;   // pop からのみアクセス
    UDPReceiver* udp;      // receive のみからアクセス

    size_t current_num_data; // 双方からアクセス 同期処理を行う
    size_t tmp_num_data;

    std::mutex mtx;
    std::condition_variable cond;

    link_list buf_list[NUM_BUFFER];
    // 体感では link_list のメンバに配列をもたせたほうが NUM_BUFFER が小さい値でも安定する(要検証)
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
            cond.wait(lk, [this] { return current_num_data > 0; });
            // const size_t up_limit = current_num_data;
            up_limit = current_num_data;
            lk.unlock();
            // size_t num_dest = 0;
            // pred が true になるか，データがなくなるまでパケットを破棄
            // アクセスできるデータがなくなったら，アクセスできるデータ数を更新
            while (num_dest < up_limit) {
                link_list* const popping = next_pop;
                ++num_dest;
                popping->data_size = 0;
                next_pop           = popping->next_ptr;
                if (pred(popping->data)) {
                    lk.lock();
                    current_num_data -= num_dest;
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
