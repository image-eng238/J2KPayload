#include "leaky_bucket_buf.hpp"
#include "opt_macro.hpp"

#include <cstring>
#include <cassert>
#include <thread>

static constexpr bool NO_BLOCKING_MTX = false;

leaky_bucket_buf::leaky_bucket_buf(UDPReceiver* const ptr, link_list* const buf, size_t len)
    : next_write{buf}, next_pop{buf}, last_write{nullptr}, udp{ptr}, current_num_data{}, tmp_num_data{}, noblocking_pop{}, buffer_length{len}, buf_list{buf}, mtx{}, cond{} {
    for (size_t i = 0; i < len - 1; ++i) {
        buf_list[i].next_ptr      = &buf_list[i + 1];
        buf_list[i].serial_number = i;
    }
    buf_list[len - 1].next_ptr = buf_list;
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
        }
        if (errno == EINTR) {
            inspkt();
            return SIGNAL;
        } else {
            perror("receive error");
            return FINISH;
        }
    }

    int output = (writing->data[0] & 0x80) ? RECEIVED : FINISH;

    last_write = writing;
    link_list::advance(next_write);

    std::unique_lock lk(mtx, std::defer_lock);
    if constexpr (NO_BLOCKING_MTX) {
        if (lk.try_lock()) {
            current_num_data += 1 + tmp_num_data;
            if (tmp_num_data != 0) tmp_num_data = 0;
            current_num_data = std::min(current_num_data, buffer_length);
            // assert(current_num_data < NUM_BUFFER);
            lk.unlock();
            cond.notify_one();
        } else {
            ++tmp_num_data;
        }
    } else {
        lk.lock();
        if (++current_num_data > buffer_length) {
            link_list::advance(next_pop);
            current_num_data = buffer_length;
        }
        lk.unlock();
        cond.notify_one();
    }
    return output;
}
void leaky_bucket_buf::inspkt() {
    auto* writing = next_write;
    std::unique_lock lk{mtx};
    if constexpr (NO_BLOCKING_MTX) {
        current_num_data += 1 + tmp_num_data;
    } else {
        if (++current_num_data > buffer_length) {
            link_list::advance(next_pop);
            current_num_data = buffer_length;
        }
    }
    next_write         = writing->next_ptr;
    last_write         = writing;
    writing->data_size = 1;
    writing->data[0]   = 0;
    lk.unlock();
    cond.notify_one();
}
void leaky_bucket_buf::push(const uint8_t* const src, const int len) {

    auto* writing = next_write;
    LOAD_INTO_CACHE(writing, opt_macro::WRITE, opt_macro::HIGH_TEMPORAL);
    // assert(writing->empty());

    writing->data_size = len;
    memcpy(writing->data, src, len);

    last_write = writing;
    link_list::advance(next_write);

    std::unique_lock lk(mtx, std::defer_lock);
    if constexpr (NO_BLOCKING_MTX) {
        if (lk.try_lock()) {
            current_num_data += 1 + tmp_num_data;
            if (tmp_num_data != 0) tmp_num_data = 0;
            current_num_data = std::min(current_num_data, buffer_length);
            // assert(current_num_data < NUM_BUFFER);
            lk.unlock();
            cond.notify_one();
        } else {
            ++tmp_num_data;
        }
    } else {
        lk.lock();
        if (++current_num_data > buffer_length) {
            link_list::advance(next_pop);
            current_num_data = buffer_length;
        }
        lk.unlock();
        cond.notify_one();
    }
}

int leaky_bucket_buf::pop(uint8_t*& ptr) {
    const auto out = pop_impl(ptr);
    if (out == 0) throw buffer_leak("empty paket popping", buffer_leak::EMPTY_POP);
    return out;
}
int leaky_bucket_buf::pop_noexcept(uint8_t*& ptr) noexcept { return pop_impl(ptr); }
int leaky_bucket_buf::pop_impl(uint8_t*& ptr) {
    auto pred = [this]() -> bool { return current_num_data > 0; };
    std::unique_lock lk(mtx, std::defer_lock);
    if constexpr (NO_BLOCKING_MTX) {
        if (noblocking_pop != 0) {
            --noblocking_pop;
        } else {
            lk.lock();
            cond.wait(lk, pred);

            noblocking_pop   = current_num_data - 1;
            current_num_data = 0;
        }
    } else {
        lk.lock();
        cond.wait(lk, pred);
        --current_num_data;
    }
    auto popping       = next_pop;
    auto out           = popping->data_size;
    ptr                = popping->data;
    popping->data_size = 0;

    link_list::advance(next_pop);

    return out;
}

void leaky_bucket_buf::clear() {
    std::unique_lock lk{mtx};
    next_write       = buf_list;
    next_pop         = buf_list;
    current_num_data = 0;
    tmp_num_data     = 0;
}
