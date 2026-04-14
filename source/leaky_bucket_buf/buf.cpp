#include "leaky_bucket_buf.hpp"

#include <cstring>
#include <cassert>

leaky_bucket_buf::leaky_bucket_buf()
    : buf_list{},
      next_write{buf_list}, next_pop{buf_list}, udp{},
      mtx{}, can_pop{}, can_receive{}, current_num_data{} {

    for (size_t i = 0; i < NUM_BUFFER - 1; ++i) {
        buf_list[i].next_ptr = &buf_list[i + 1];
    }
    buf_list[NUM_BUFFER - 1].next_ptr = buf_list;
}
leaky_bucket_buf::leaky_bucket_buf(UDPReceiver* const ptr) : leaky_bucket_buf{} {
    set_udp(ptr);
}

constexpr void leaky_bucket_buf::set_udp(UDPReceiver* const ptr) {
    this->udp = ptr;
}

void leaky_bucket_buf::receive() {

    std::unique_lock lk(mtx);
    can_receive.wait(lk, [this] {
        assert(current_num_data < NUM_BUFFER);
        return current_num_data < NUM_BUFFER;
    });

    auto& writing = next_write;
    assert(writing->empty());
    writing->data_size = udp->receive(writing->data, BUFFER_SIZE);

    next_write = writing->next_ptr;

    // ここで writing と next_ptr の順序を確認し，ソートする．
    // RTP の場合はシーケンス番号をチェックしてソート．

    ++current_num_data;
    can_pop.notify_all();
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
    can_pop.wait(lk, [this] {
        return current_num_data != 0;
    });

    auto popping       = next_pop;
    auto out           = popping->data_size;
    ptr                = popping->data;
    popping->data_size = 0;

    next_pop = popping->next_ptr;

    --current_num_data;
    can_receive.notify_all();

    return out;
}