#include "leaky_bucket_buf.hpp"
#include "opt_macro.hpp"

#include <cstring>
#include <cassert>
#include <thread>

#define PRINT_ASSERTION(expr, msg, ...) assert(((expr) ? true : (printf("assertion message: " msg, __VA_ARGS__), false)))

leaky_bucket_buf::leaky_bucket_buf()
    : next_write{buf_list[0]}, udp{}, tmp_num_data{},
      current_buffer{accesser}, current_buffer_pos{}, buf_list{}, accesser{} {

    for (size_t i = 0; i < NUM_BUFFER; ++i) {
        for (size_t j = 0; j < BUFFER_LENGTH - 1; ++j) {
            buf_list[i][j].next_ptr = &buf_list[i][j + 1];
        }
        buf_list[i][BUFFER_LENGTH - 1].next_ptr = buf_list[i];
        accesser[i].next_pop                    = buf_list[i];
    }
}
leaky_bucket_buf::leaky_bucket_buf(UDPReceiver* const ptr) : leaky_bucket_buf{} {
    set_udp(ptr);
}

constexpr void leaky_bucket_buf::set_udp(UDPReceiver* const ptr) {
    this->udp = ptr;
}

bool leaky_bucket_buf::receive() {

    auto writing = this->next_write;
    LOAD_INTO_CACHE(writing, opt_macro::WRITE, opt_macro::HIGH_TEMPORAL);
    assert(writing->empty());

    // パケットを受信してバッファに書き込み
    writing->data_size = static_cast<int>(udp->receive(writing->data, ring_element::PACKET_SIZE));

    // 受信結果の確認
    if (writing->data_size == -1) {
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

    // 受信終了用のパケットの確認
    bool output = writing->data[0] & 0x80;

    std::unique_lock lk(current_buffer->mtx, std::defer_lock);
    if (unlikely(writing->data[1] & 0x80)) { // パケットに J2K EOC が含まれているか
        // 書き込み中のバッファの終了処理
        lk.lock();
        current_buffer->num_data += 1 + tmp_num_data;
        if (tmp_num_data != 0) tmp_num_data = 0;
        lk.unlock();
        current_buffer->cond.notify_one();

        // 書き込みバッファを更新
        current_buffer_pos   = (current_buffer_pos != NUM_BUFFER) ? current_buffer_pos + 1 : 0;
        this->current_buffer = &this->accesser[current_buffer_pos];
        assert(current_buffer->num_data == 0);
        this->tmp_num_data = 0;
        this->next_write   = current_buffer->next_pop;

        return output;
    }

    this->next_write = writing->next_ptr;
    if (lk.try_lock()) {
        current_buffer->num_data += 1 + tmp_num_data;
        if (tmp_num_data != 0) tmp_num_data = 0;
        lk.unlock();
        current_buffer->cond.notify_one();
    } else {
        ++tmp_num_data;
    }
    return output;
}
