#include "UDP.hpp"

#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <charconv>

#include <chrono>

enum {
    SENDER   = 0,
    RECIEVER = 1
};

void sender(const char* const address, const uint16_t port) {
    UDPSender udp(address, port);
    std::string str;
    std::cout << "send string: " << std::endl;
    std::cin >> str;
    udp.send(str.data(), str.length());
}

void reciever(const char* const address, const uint16_t port) {
    UDPReceiver udp(address, port);
    char str[255] = {};
    str[255]      = 0;

    std::cout << "wait..." << std::endl;
    udp.receive(str, 254);
    std::cout << "recv string: " << str << std::endl;
}

int main(int argc, char** argv) {
    std::vector<std::string_view> arg_view(argc - 1);
    for (int i = 1; i < argc; ++i) {
        arg_view[i - 1] = argv[i];
    }

    std::string_view addr;
    uint16_t port = 0;
    uint8_t type  = SENDER;
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
        if (*it == "-t") {
            it++;
            if (std::from_chars(it->begin(), it->end(), type).ptr != it->end()) {
                type = SENDER;
            }
        }
    }

    switch (type) {
        case SENDER:
            sender(addr.data(), port);
            break;

        case RECIEVER:
            reciever(addr.data(), port);
            break;

        default:
            std::cout << "error -t: " << static_cast<int>(type) << std::endl;
            break;
    }

    return 0;
}