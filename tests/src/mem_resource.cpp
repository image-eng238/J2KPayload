#include "mem_resource.hpp"
#include <vector>

int main() {
    j2k_resource<int, char, size_t> memory;
    if (!memory.prev_allocate(16, 2, 3, std::nothrow)) {
        exit(1);
    }
    {
        std::pmr::vector<int> vec_int(&memory);
        std::pmr::vector<int> vec_int2(&memory);
        vec_int2.reserve(4);
        vec_int.reserve(8);
        std::pmr::vector<char> vec_char(&memory);
        vec_char.reserve(2);
        std::pmr::vector<size_t> vec_size_t(&memory);
        vec_size_t.reserve(3);
    }

    return 0;
}