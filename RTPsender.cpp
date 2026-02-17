#include "UDP.hpp"
#include "RTP_file_format.hpp"

int main() {
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
    return 0;
}