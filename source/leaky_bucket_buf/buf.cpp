#include "leaky_bucket_buf.hpp"
#include "opt_macro.hpp"

#include <cstring>
#include <cassert>
#include <thread>

#define PRINT_ASSERTION(expr, msg, ...) assert(((expr) ? true : (printf("assertion message: " msg, __VA_ARGS__), false)))

leaky_bucket_buf::leaky_bucket_buf()
    : next_write{buf_list}, next_pop{buf_list}, last_receive{nullptr}, udp{}, current_num_data{}, tmp_num_data{}, mtx{}, cond{}, buf_list{} {

    for (size_t i = 0; i < NUM_BUFFER - 1; ++i) {
        buf_list[i].next_ptr = &buf_list[i + 1];
        // buf_list[i].data_size = 0;
        // buf_list[i].data     = &buffer[i * BUFFER_SIZE];
    }
    buf_list[NUM_BUFFER - 1].next_ptr = buf_list;
    // buf_list[NUM_BUFFER - 1].data_size = 0;
    // buf_list[NUM_BUFFER - 1].data     = &buffer[(NUM_BUFFER - 1) * BUFFER_SIZE];
}
leaky_bucket_buf::leaky_bucket_buf(UDPReceiver* const ptr) : leaky_bucket_buf{} {
    set_udp(ptr);
}

constexpr void leaky_bucket_buf::set_udp(UDPReceiver* const ptr) {
    this->udp = ptr;
}

int leaky_bucket_buf::receive() {

    auto* writing = next_write;
    LOAD_INTO_CACHE(writing, opt_macro::WRITE, opt_macro::HIGH_TEMPORAL);
    // assert(writing->empty());

    writing->data_size = static_cast<int>(udp->receive(writing->data, BUFFER_SIZE));
    if (writing->data_size == -1) {
        if (BRANCH_PROB(errno == EAGAIN, 1.0)) {
            return AGAIN;
        } else {
            perror("receive error");
            return FINISH;
        }
    }

    int output = (writing->data[0] & 0x80) ? RECEIVED : FINISH;

    next_write   = writing->next_ptr;
    last_receive = writing;

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

int leaky_bucket_buf::pop(uint8_t*& ptr) {
    // while (current_num_data.load(std::memory_order_relaxed) <= 0) {
    //     std::this_thread::yield();
    // }

    std::unique_lock lk(mtx);
    cond.wait(lk, [this] { return current_num_data > 0; });

    auto popping = next_pop;
    auto out     = popping->data_size;
    ptr          = popping->data;
    // popping->data_size = 0;

    next_pop = popping->next_ptr;

    --current_num_data;
    lk.unlock();

    return out;
}

void leaky_bucket_buf::clear() {
    std::unique_lock lk{mtx};
    next_write       = buf_list;
    next_pop         = buf_list;
    current_num_data = 0;
    tmp_num_data     = 0;
}