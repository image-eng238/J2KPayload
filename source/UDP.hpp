#pragma once
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <iostream>

class UDPBase {
public:
    UDPBase() {
        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock == -1) {
            std::cout << "socket error" << std::endl;
            exit(1);
        }

        socket_address = []() constexpr {
            sockaddr_in out{};
            out.sin_family      = AF_INET;
            out.sin_port        = htons(50001);
            out.sin_addr.s_addr = inet_addr("127.0.0.1");
            return out;
        }();
    }
    ~UDPBase() {
        if (sock != -1) close(sock);
    }

protected:
    int sock;
    sockaddr_in socket_address;
};

class UDPSender : public UDPBase {
public:
    UDPSender() {}
    bool send(const void* const buf_ptr, const size_t buf_size) {
        if (sendto(sock, buf_ptr, buf_size, 0, reinterpret_cast<const sockaddr*>(&socket_address), sizeof(sockaddr_in)) == -1) {
            std::cout << "send error" << std::endl;
            return false;
        }
        return true;
    }
};

class UDPReceiver : public UDPBase {
public:
    UDPReceiver() {
        if (bind(sock, reinterpret_cast<const sockaddr*>(&socket_address), sizeof(sockaddr_in)) == -1) {
            std::cout << "bind error" << std::endl;
            exit(1);
        }
    }
    int receive(void* const buf_ptr, const size_t buf_size) {
        auto output = recv(sock, buf_ptr, buf_size, 0);
        if (output == -1) {
            std::cout << "receive error" << std::endl;
        }
        return output;
    }
};