#pragma once
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <iostream>

class UDPBase {
public:
    UDPBase() {
        // sock = socket(AF_INET, SOCK_DGRAM, 0);
        // if (sock == -1) {
        //     std::cout << "socket error" << std::endl;
        //     exit(1);
        // }

        // socket_address = []() constexpr {
        //     sockaddr_in out{};
        //     out.sin_family      = AF_INET;
        //     out.sin_port        = htons(50001);
        //     out.sin_addr.s_addr = inet_addr("127.0.0.1");
        //     return out;
        // }();
    }
    ~UDPBase() {
        if (sock != -1) close(sock);
    }
    auto get_sock_fd() const { return sock; }

protected:
    int sock;
    sockaddr_in socket_address;
};

class UDPSender : public UDPBase {
public:
    UDPSender() {}
    UDPSender(const char* const address, const uint16_t port) {
        if (!make_sock(address, port)) {
            exit(1);
        }
    }
    bool make_sock(const char* const address = "127.0.0.1", const uint16_t port = 50001) {
        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock == -1) {
            perror("socket");
            return false;
        }
        socket_address = [&]() {
            sockaddr_in out{};
            out.sin_family      = AF_INET;
            out.sin_port        = htons(port);
            out.sin_addr.s_addr = inet_addr(address);
            return out;
        }();
        return true;
    }
    auto send(const void* const buf_ptr, const size_t buf_size) {
        auto output = sendto(sock, buf_ptr, buf_size, 0, reinterpret_cast<const sockaddr*>(&socket_address), sizeof(sockaddr_in));
        return output;
    }
};

class UDPReceiver : public UDPBase {
public:
    UDPReceiver() : msg{nullptr, 0, &iov, 1, cmsg_buffer, cmsg_len, 0}, iov{}, cmsg_buffer{}, prev_overflow{} {}
    UDPReceiver(const char* const address, const uint16_t port) : UDPReceiver{} {
        if (!sock_bind(address, port)) {
            exit(1);
        }
    }
    bool sock_bind(const char* const address = "127.0.0.1", const uint16_t port = 50001);
    ssize_t receive(void* const buf_ptr, const size_t buf_size);
    uint32_t get_overflow_packet();
    timespec get_timestamp();

private:
    static constexpr size_t cmsg_len = 1024;
    msghdr msg;
    iovec iov;
    uint8_t cmsg_buffer[cmsg_len];
    uint32_t prev_overflow;
};