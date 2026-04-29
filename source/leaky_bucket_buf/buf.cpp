#include "leaky_bucket_buf.hpp"

#include <cstring>
#include <cassert>
#include <thread>

#define PRINT_ASSERTION(expr, msg, ...) assert(((expr) ? true : (printf("assertion message: " msg, __VA_ARGS__), false)))

leaky_bucket_buf::leaky_bucket_buf()
    : next_write{buf_list}, next_pop{buf_list}, udp{}, current_num_data{}, buf_list{} {

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

    if (tmp_data_size == -1) {
        if (errno == EAGAIN) {
            return true;
        } else {
            perror("receive error");
            return false;
        }
    }

    assert(current_num_data < NUM_BUFFER); // buffer leak

    auto& writing = next_write;

    memcpy(writing->data, tmp_buf, tmp_data_size);
    writing->data_size = tmp_data_size;
    if (!(writing->data[0] & 0x80)) {
        ++current_num_data;
        return false;
    }

    // ここで writing と next_ptr の順序を確認し，ソートする．
    // RTP の場合はシーケンス番号をチェックしてソート．

    next_write = writing->next_ptr;

    ++current_num_data;

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
    while (current_num_data.load(std::memory_order_relaxed) <= 0) {
        std::this_thread::yield();
    }

    auto popping       = next_pop;
    auto out           = popping->data_size;
    ptr                = popping->data;
    popping->data_size = 0;

    next_pop = popping->next_ptr;

    --current_num_data;

    return out;
}