#include "UDP.hpp"

#include <cstring>

bool UDPReceiver::sock_bind(const char* const address, const uint16_t port) {
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
    // ブロッキングモードの指定
    // int val = 1;
    // if (ioctl(sock, FIONBIO, &val) == -1) {
    //     perror("ioctl(FIONBIO)");
    //     return false;
    // }

    // cmsg にオーバーフローしたデータ数の合計(uint32_t)を要求
    linger optval_ovfl{1, 0};
    if (setsockopt(sock, SOL_SOCKET, SO_RXQ_OVFL, &optval_ovfl, sizeof(optval_ovfl)) == -1) {
        perror("setsockopt(SO_RXQ_OVFL)");
        return false;
    }

    // cmsg に受信時のタイムスタンプを要求
    int val_timestampns = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_TIMESTAMPNS, &val_timestampns, sizeof(val_timestampns)) == -1) {
        perror("setsockopt(SO_TIMESTAMP)");
        return false;
    }

    if (bind(sock, reinterpret_cast<const sockaddr*>(&socket_address), sizeof(sockaddr_in)) == -1) {
        perror("bind");
        return false;
    }

    return true;
}

ssize_t UDPReceiver::receive(void* const buf_ptr, const size_t buf_size) {
    msg.msg_iov->iov_base = buf_ptr;
    msg.msg_iov->iov_len  = buf_size;
    auto output           = recvmsg(sock, &msg, 0);
    return output;
}

uint32_t UDPReceiver::get_overflow_packet() {
    for (cmsghdr* cmsg = CMSG_FIRSTHDR(&msg); cmsg != nullptr; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_type == SO_RXQ_OVFL) {
            return *(uint32_t*)CMSG_DATA(cmsg);
        }
    }
    return 0;
}

timespec UDPReceiver::get_timestamp() {
    timespec tmp = {};
    for (cmsghdr* cmsg = CMSG_FIRSTHDR(&msg); cmsg != nullptr; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_TIMESTAMPNS) {
            memcpy(&tmp, CMSG_DATA(cmsg), sizeof(tmp));
        }
    }
    return tmp;
}
