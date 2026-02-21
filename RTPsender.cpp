// およそ16msごとにパケットを送信

#include "UDP.hpp"
#include "RTP_file_format.hpp"

#include <thread>

void send_task() {
    const char* const FILE_PATH = "./data/2160p_5994fps_422_10bit.rtp";
    UDPSender s;
    RTPFile rtp(FILE_PATH);
    uint8_t send_buf[BUFSIZ] = {};
    while (true) {
        auto send_size = rtp.get_packet(send_buf);
        if (send_size == 0) {
            exit(1);
        }
        if (!s.send(send_buf, send_size)) {
            break;
        }
    }
    char end = 0;
    s.send(&end, 1);
}

void clock_task(const size_t ck_ms) {}

int main() {
    const size_t send_clock = 16;

    std::thread send_thread(send_task);
    std::thread clock_thread(clock_task, send_clock);

    send_thread.join();
    clock_thread.join();

    return 0;
}