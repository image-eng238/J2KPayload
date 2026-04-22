#include "UDP.hpp"

#include <vector>
#include <string_view>
#include <charconv>
#include <cassert>

int main(int argc, char** argv) {
    std::vector<std::string_view> arg_view(argc - 1);
    for (int i = 1; i < argc; ++i) {
        arg_view[i - 1] = argv[i];
    }

    std::string_view addr;
    uint16_t port = 0;
    for (auto it = arg_view.begin(); it != arg_view.end(); ++it) {

        if (*it == "-a") {
            it++;
            addr = it->data();
        }
        if (*it == "-p") {
            it++;
            if (std::from_chars(it->begin(), it->end(), port).ptr != it->end()) {
                port = 0;
            }
        }
    }

    UDPSender udp(addr.data(), port);
    uint8_t c = 0;
    udp.send(&c, 1);

    printf("stop_j2kpay\n");

    return 0;
}