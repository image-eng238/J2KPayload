#include "packet_t.hpp"
template <typename T, typename L>
struct vec {
    T* ptr;
    L len;
};

int main(void) {
    {
        uint8_t stor[16]{};
        size_t len = 16;
        pointer_with_length<uint8_t, const size_t&> pkt{stor, len};
        auto l  = pkt.size();
        auto ll = pkt.size();
        // pkt.len = 12;
    }
    {
        std::byte buffer[16];
        std::size_t length = 16;

        vec<std::byte, std::size_t&> v{buffer, length};

        v.len  = 15;
        length = 14;
        v.len  = 13;
        length = 12;
    }
    return 0;
}