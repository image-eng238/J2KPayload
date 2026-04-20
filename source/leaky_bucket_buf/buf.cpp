#include "leaky_bucket_buf.hpp"

#include <cstring>
#include <cassert>
#include <thread>

#define PRINT_ASSERTION(expr, msg, ...) assert(((expr) ? true : (printf("assertion message: " msg, __VA_ARGS__), false)))

leaky_bucket_buf::leaky_bucket_buf()
    : buf_list{},
      next_write{buf_list}, next_pop{buf_list}, udp{},
      mtx{}, can_pop{}, can_receive{}, current_num_data{} {

    for (size_t i = 0; i < NUM_BUFFER - 1; ++i) {
        buf_list[i].next_ptr = &buf_list[i + 1];
        // buf_list[i].data     = &buffer[i * BUFFER_SIZE];
    }
    buf_list[NUM_BUFFER - 1].next_ptr = buf_list;
    // buf_list[NUM_BUFFER - 1].data     = &buffer[(NUM_BUFFER - 1) * BUFFER_SIZE];
}
leaky_bucket_buf::leaky_bucket_buf(UDPReceiver* const ptr) : leaky_bucket_buf{} {
    set_udp(ptr);
}

constexpr void leaky_bucket_buf::set_udp(UDPReceiver* const ptr) {
    this->udp = ptr;
}

bool leaky_bucket_buf::receive() {
    uint8_t tmp_buf[BUFFER_SIZE];
    int tmp_data_size = udp->receive(tmp_buf, BUFFER_SIZE);

    if (tmp_data_size == -1) { return false; }

    static uint32_t pre_seq = 0;
    const uint32_t recv_seq = get_seq(tmp_buf);
    PRINT_ASSERTION(((recv_seq == pre_seq + 1) || (pre_seq == 0) || (recv_seq == 0)), "now: %d, pre: %d, diff: %d\n", recv_seq, pre_seq, recv_seq - pre_seq);
    pre_seq = recv_seq;

    {
        std::unique_lock lk(mtx);
        // can_receive.wait(lk, [this] {
        //     return current_num_data < NUM_BUFFER;
        // });
        assert(current_num_data < NUM_BUFFER);

        auto& writing = next_write;
        assert(writing->empty());
        // writing->data_size = udp->receive(writing->data, BUFFER_SIZE);
        memcpy(writing->data, tmp_buf, tmp_data_size);
        writing->data_size = tmp_data_size;
        if (!(writing->data[0] & 0x80)) {
            ++current_num_data;
            can_pop.notify_all();
            return false;
        }

        // ここで writing と next_ptr の順序を確認し，ソートする．
        // RTP の場合はシーケンス番号をチェックしてソート．

        next_write = writing->next_ptr;

        ++current_num_data;
        // current_num_data.fetch_add(1, std::memory_order_relaxed);
        // static size_t c = 0;
        // printf("r: %ld\n", c++);
        can_pop.notify_all();
    }
    return true;
}

void leaky_bucket_buf::write(const uint8_t* const src, const size_t& len) {
    auto& writing = next_write;
    auto ast      = writing->empty();
    assert(ast); // ここでプログラムが停止する場合，データの排出が追い付いてない
    memcpy(writing->data, src, len);
    writing->data_size = len;

    next_write = writing->next_ptr;
}

uint8_t* leaky_bucket_buf::pop() {
    // assert(!next_pop->empty());
    while (next_pop->empty()); // データの排出が入力より速いため，データが入力されるまで待機
    auto popping       = next_pop;
    popping->data_size = 0;

    next_pop = popping->next_ptr;
    return popping->data;
}

int leaky_bucket_buf::pop(uint8_t*& ptr) {
    // assert(!next_pop->empty());
    // while (next_pop->empty()); // データの排出が入力より速いため，データが入力されるまで待機

    std::unique_lock lk(mtx);

    const bool USE_TIME_OUT = false;

    if constexpr (USE_TIME_OUT) {
        static bool is_called = false;
        if (is_called) {
            const int64_t timeout_ms = 10000;
            auto result              = can_pop.wait_for(lk, std::chrono::milliseconds(timeout_ms), [this] {
                return current_num_data != 0;
            });
            if (result == static_cast<bool>(std::cv_status::timeout)) {
                std::cout << "can_pop.wait_for(" << timeout_ms << "ms): timeout" << std::endl;
                exit(1);
            }
        } else {
            can_pop.wait(lk, [this] {
                return current_num_data != 0;
            });
            is_called = true;
        }
    } else {
        can_pop.wait(lk, [this] {
            return current_num_data != 0;
        });
    }

    // while (current_num_data.load(std::memory_order_relaxed) == 0) {
    // while (current_num_data == 0) {
    //     std::this_thread::yield();
    // }

    // if (current_num_data.load(std::memory_order_relaxed) == 0) {
    //     std::unique_lock<std::mutex> ul(mtx);
    //     can_pop.wait(ul);
    // }

    auto popping       = next_pop;
    auto out           = popping->data_size;
    ptr                = popping->data;
    popping->data_size = 0;

    next_pop = popping->next_ptr;

    --current_num_data;
    // current_num_data.fetch_sub(1, std::memory_order_relaxed);
    // static size_t c = 0;
    // printf("      p: %ld\n", c++);
    can_receive.notify_all();

    return out;
}