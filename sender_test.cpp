#include "UDP.hpp"
#include "RTP_header.hpp"
#include "leaky_bucket_buf.hpp"

#include <thread>
#include <string>
#include <string_view>
#include <vector>
#include <charconv>

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

    UDPReceiver upd(addr.data(), port);
    // std::thread t1([&] {
    uint8_t recv_buf[leaky_bucket_buf::PACKET_SIZE];
    RTPHeader rtp(recv_buf);
    J2KPayloadHeader j2k(recv_buf + RTPHeader::get_header_length());

    uint32_t pre     = 0;
    size_t count_err = 0;

    while (true) {
        // std::this_thread::yield();
        auto siz = upd.receive(recv_buf, leaky_bucket_buf::PACKET_SIZE);
        if (siz == -1 && errno != EAGAIN) {
            perror("receive error");
            break;
        }
        if (siz == -1) continue;
        if (siz == 1) break;
        // auto sq = leaky_bucket_buf::get_seq(recv_buf);
        auto sq = RTPReceiver::get_extended_sequence_number(recv_buf);
        // assert(sq == pre + 1 || pre == 0);
        if (!(sq == pre + 1 || pre == 0)) {
            printf("%d", sq);
            printf(": error diff: %d, receive size: %d type: %x\n", sq - pre, siz, j2k.get_MH());
            ++count_err;
        } else {
            // puts("");
        }
        pre = sq;
    }
    // });

    // t1.join();
    printf("count_err: %ld\n", count_err);

    return 0;
}