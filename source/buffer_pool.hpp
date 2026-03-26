#pragma once

#include <cstdint>
#include <cstddef>

template <typename T, size_t Buffersize, size_t Numbuffer>
class BufferPool {
public:
    T* get() {
        for (size_t i = 0; i < Buffersize; ++i) {
            if (is_use[i] == false) {
                is_use[i] = true;
                return buffer[i];
            }
        }
        return nullptr;
    }
    void release(T* ptr) {
        for (size_t i = 0; i < Buffersize; ++i) {
            if (ptr == buffer[i]) {
                is_use[i] = false;
                return;
            }
        }
    }

private:
    T buffer[Numbuffer][Buffersize];
    bool is_use[Numbuffer];
};