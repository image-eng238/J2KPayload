#include "leaky_bucket_buf.hpp"

int lbb_access::pop(uint8_t*& ptr) {
    std::unique_lock lk(mtx);
    cond.wait(lk, [this] { return num_data > 0; });

    auto popping       = next_pop;
    auto out           = popping->data_size;
    ptr                = popping->data;
    popping->data_size = 0;

    next_pop = popping->next_ptr;

    --num_data;
    lk.unlock();

    return out;
}