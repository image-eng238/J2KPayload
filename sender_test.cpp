#include "UDP.hpp"
#include "RTP_header.hpp"
#include "leaky_bucket_buf.hpp"

#include <thread>

int main(void) {
    UDPReceiver upd("127.0.0.1", 50001);
    // std::thread t1([&] {
    uint8_t recv_buf[leaky_bucket_buf::BUFFER_SIZE];
    RTPHeader rtp(recv_buf);
    J2KPayloadHeader j2k(recv_buf + RTPHeader::get_header_length());

    uint32_t pre = 0;

    while (true) {
        // std::this_thread::yield();
        auto siz = upd.receive(recv_buf, leaky_bucket_buf::BUFFER_SIZE);
        if (siz == -1 && errno != EAGAIN) {
            perror("receive error");
            break;
        }
        if (siz == -1) continue;
        if (siz == 1) break;
        auto sq = leaky_bucket_buf::get_seq(recv_buf);
        // assert(sq == pre + 1 || pre == 0);
        printf("%d", sq);
        if (!(sq == pre + 1 || pre == 0)) {
            printf(": error diff: %d, receive size: %d type: %x\n", sq - pre, siz, j2k.get_MH());
        } else {
            puts("");
        }
        pre = sq;
    }
    // });

    // t1.join();

    return 0;
}