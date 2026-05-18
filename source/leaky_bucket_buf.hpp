#pragma once

#include "UDP.hpp"
#include "opt_macro.hpp"

#include <array>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>

class leaky_bucket_buf {

public:
#ifdef GENERATE_RECEIVE_PROBABILITY
    inline static size_t count_receive = 0;
    inline static size_t count_agaein  = 0;
#endif
    static constexpr size_t J2K_PACKET_SIZE = 1364;
    static constexpr size_t HEADER_LENGHT   = 20;
    static constexpr size_t BUFFER_SIZE     = 1384;
    static constexpr size_t NUM_BUFFER      = 3000;
    static constexpr size_t WAIT_PACKET     = 3;
    leaky_bucket_buf();
    leaky_bucket_buf(UDPReceiver* const);

    constexpr void set_udp(UDPReceiver* const);
    int pop(uint8_t*&);
    static uint32_t get_seq(const uint8_t* const data) { return static_cast<uint32_t>(data[15] << 0x10) | static_cast<uint32_t>(data[2] << 0x8) | static_cast<uint32_t>(data[3]); }
    size_t get_num_data() {
        std::unique_lock lk{mtx};
        return current_num_data + tmp_num_data;
    }
    size_t get_num_data_unsafe() const { return current_num_data + tmp_num_data; }

private:
    struct link_list {
        int data_size;
        uint8_t* data;
        link_list* next_ptr;
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
    std::array<link_list*, WAIT_PACKET> wait_packet;
    size_t num_wait_packet;
    uint8_t buffer[NUM_BUFFER][J2K_PACKET_SIZE];

public:
    template <typename Predcate>
    bool receive(Predcate pred) {
        // bool receive() {

        uint8_t tmp_buf[BUFFER_SIZE];
        int tmp_data_size = udp->receive(tmp_buf, BUFFER_SIZE);

        if (tmp_data_size == -1) {
            if (BRANCH_PROB(errno == EAGAIN, 1.0)) {
#ifdef GENERATE_RECEIVE_PROBABILITY
                ++count_agaein;
#endif
                return true;
            } else {
                perror("receive error");
                return false;
            }
        }
#ifdef GENERATE_RECEIVE_PROBABILITY
        ++count_receive;
#endif

        // assert(current_num_data + tmp_num_data < NUM_BUFFER); // buffer leak

        auto& writing = next_write;
        LOAD_INTO_CACHE(writing, opt_macro::WRITE, opt_macro::HIGH_TEMPORAL);
        // assert(writing->empty());

        // ここで RTP パケットに関する処理を行い，
        // 共有バッファには J2K コードストリームのデータのみを格納
        // シーケンスエラーが起こった場合，数パケットだけ待機して，パケット順の前後に対応．
        // 待機後にエラーが起こったパケットが現れなかった場合パケットロスとして，
        // そのフレームを破棄
        if (pred(tmp_buf)) {
            if (num_wait_packet != 0) num_wait_packet = 0;
        } else {
            if (num_wait_packet == WAIT_PACKET) {
                // 破棄処理]
                num_wait_packet = 0;
                writing;
            } else {
                wait_packet[num_wait_packet++] = writing;
            }
            return true;
        }

        memcpy(writing->data, tmp_buf + HEADER_LENGHT, tmp_data_size - HEADER_LENGHT);
        writing->data_size = tmp_data_size - HEADER_LENGHT;

        bool output = writing->data[0] & 0x80;

        next_write = writing->next_ptr;

        std::unique_lock lk(mtx, std::defer_lock);
        if (lk.try_lock()) {
            current_num_data += 1 + tmp_num_data;
            if (tmp_num_data != 0) tmp_num_data = 0;
            // assert(current_num_data < NUM_BUFFER);
            lk.unlock();
            cond.notify_one();
        } else {
            ++tmp_num_data;
            // スレッドセーフでないが current_num_data は他スレッドから操作は減算のみであるためアサーションに使用
            // assert(current_num_data + tmp_num_data < NUM_BUFFER);
        }
        return output;
    }

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
                callback(popping->data);
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
