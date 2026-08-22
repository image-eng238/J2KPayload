#include "mem_resource.hpp"
#include <vector>

class Defer;

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
    {
        // j2k_resource<Defer> m; 型の定義に完全型が必要
    }
    {
        j2k_parent_resource<size_t, int, char> mem;
        mem.prev_allocate(12, 4, 16);
        {
            std::pmr::vector<size_t> vec{mem.get_resource<0>()};
            vec.reserve(12);
            std::pmr::vector<int> veci{mem.get_resource<1>()};
            vec.reserve(4);
            std::pmr::vector<char> vecc{mem.get_resource<2>()};
            vecc.reserve(16);
        }
        {
            std::pmr::vector<size_t> vec{mem.get_resource<0>()};
            vec.reserve(12);
            std::pmr::vector<int> veci{mem.get_resource<1>()};
            vec.reserve(4);
            std::pmr::vector<char> vecc{mem.get_resource<2>()};
            vecc.reserve(16);
        }
    }
    {
        j2k_parent_resource<virtual_type_t<16, 8>, virtual_type_t<1, 1>> mem{3, 16};
        std::pmr::vector<std::pair<size_t, size_t>> vecss{mem.get_resource<0>()};
        vecss.reserve(3);
        std::pmr::vector<uint8_t> vecu8{mem.get_resource<1>()};
        vecu8.reserve(8);
        std::pmr::vector<int8_t> veci8{mem.get_resource<1>()};
        veci8.reserve(8);
    }

    return 0;
}

class Defer {};