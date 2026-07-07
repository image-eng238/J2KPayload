#pragma once

#include "UDP.hpp"

#include <cstdint>
#include <cstddef>
#include <cassert>

#include <mutex>
#include <condition_variable>

#include <atomic>

class buffer_leak : public std::runtime_error {
public:
    enum {
        OTHER,
        ANALYSISING,
        EMPTY_POP,
    };
    explicit buffer_leak(const std::string& arg) : std::runtime_error{arg}, type{OTHER} {}
    explicit buffer_leak(const std::string& arg, const int t) : std::runtime_error{arg}, type{t} {}
    int type;
};

class leaky_bucket_buf {
public:
    static constexpr size_t BUFFER_SIZE = 1384;
    static constexpr size_t NUM_BUFFER  = 10 * 1360;
    struct link_list {
        link_list* next_ptr;
        int data_size;
        uint32_t serial_number;
        uint8_t data[leaky_bucket_buf::BUFFER_SIZE];
        bool empty() const { return data_size <= 0; }
        static void advance(link_list*& ptr) { ptr = ptr->next_ptr; }
    };

    enum {
        FINISH   = -2,
        SIGNAL   = -1,
        AGAIN    = 0,
        RECEIVED = 1
    };
    leaky_bucket_buf(UDPReceiver* const, link_list* const, size_t);

    constexpr void set_udp(UDPReceiver* const);
    int receive();
    void inspkt();
    int pop(uint8_t*&);
    int pop_noexcept(uint8_t*&) noexcept;
    void clear();
    static uint32_t get_seq(const uint8_t* const data) { return static_cast<uint32_t>(data[15] << 0x10) | static_cast<uint32_t>(data[2] << 0x8) | static_cast<uint32_t>(data[3]); }
    size_t get_num_data() {
        std::unique_lock lk{mtx};
        return current_num_data + tmp_num_data;
    }
    bool empty() {
        std::unique_lock lk{mtx};
        return (current_num_data + tmp_num_data == 0) && (next_write->serial_number == next_pop->serial_number);
    }
    size_t get_num_data_unsafe() const { return current_num_data + tmp_num_data; }
    const link_list* get_last_packet() const { return last_receive; }
    size_t get_buffer_length() const { return buffer_length; }

private:
    int pop_impl(uint8_t*&);
    link_list* next_write;   // receive からのみアクセス
    link_list* next_pop;     // pop からのみアクセス
    link_list* last_receive; // おそらく receive からのみアクセス
    UDPReceiver* udp;        // receive のみからアクセス

    size_t current_num_data; // 双方からアクセス 同期処理を行う
    size_t tmp_num_data;     // 受信スレッドで mutex の取得ができないときに受信したデータ数を記録
    size_t noblocking_pop;   // 解析スレッドで mutex の取得ができないときに備えて取り出せるデータ数をコピー
    const size_t buffer_length;
    link_list* const buf_list;

    std::mutex mtx;
    std::condition_variable cond;

public:
    // 条件式 pred(uint8_t* data) を満たすまでパケットを捨てる data はパケットデータの先頭のポインタ
    // callback(uint8_t* data) で直前に破棄したバッファから必要なデータを抜きだす
    template <typename Predcate, typename Callback = void(const uint8_t* const)>
    size_t dest(Predcate pred, Callback callback = [](const uint8_t* const) -> void {}) {
        std::unique_lock lk(mtx, std::defer_lock);
        size_t dest_packet = 0;
        while (true) {
            lk.lock();
            cond.wait(lk, [this] { return current_num_data > 0; });
            const size_t up_limit = current_num_data;
            current_num_data      = 0;
            lk.unlock();
            size_t num_dest = 0;
            // pred が true になるか，データがなくなるまでパケットを破棄
            // アクセスできるデータがなくなったら，アクセスできるデータ数を更新
            while (num_dest < up_limit) {
                link_list* const popping = next_pop;
                ++num_dest;
                popping->data_size = 0;
                next_pop           = popping->next_ptr;
                if (pred(popping->data)) {
                    lk.lock();
                    callback(popping->data);
                    // current_num_data -= num_dest;
                    lk.unlock();
                    return dest_packet + num_dest;
                    // return num_dest;
                }
            }
            // lk.lock();
            // current_num_data -= num_dest;
            // lk.unlock();
            dest_packet += num_dest;
        }
    }
};
