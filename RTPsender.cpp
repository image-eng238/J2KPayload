#include "UDP.hpp"
#include "RTP_file_format.hpp"
#include "RTP_header.hpp"
#include "argument.hpp"

#include <vector>
#include <string>
#include <string_view>
#include <charconv>
#include <thread>
#include <chrono>

// ./build/bin/RTPsender -r ./data/2160p_5994fps_422_10bit.rtp -a 127.0.0.1 -p 50001 -s 1 -i 1

// 2160p_5994fps_422_10bit.rtp パケット数は 163200 2秒分のサンプルなので 1フレーム当たりのパケット数は 1360

int main(int argc, char** argv) {

    std::vector<std::string_view> arg_view(argc - 1);
    for (size_t i = 1; i < argc; ++i) {
        arg_view[i - 1] = argv[i];
    }

    std::string_view addr  = "127.0.0.1", rtp_path;
    uint16_t port          = 50001;
    int64_t interval       = 0;
    size_t skep_frame      = 0;
    int64_t allowable_time = 16;
    size_t file_loop       = 0;
    bool is_enter          = false;

    {
        using namespace tklib;
        static constexpr argument_list<9> args_list(
            {{'a', "address", "Send to IPv4 address. default: 127.0.0.1"},
             {'p', "port", "Send to port. default: 50001"},
             {'r', "rtp_file", "The .rtp file source of packet to send"},
             {'i', "interval", "Packet transmission interval (microseconds)"},
             {'s', "skep_frame", "Number of frames to skip"},
             {'f', "frame_rate", "Frame fate. default: 60"},
             {'l', "loop", "Number of loops"},
             {0, "Enter", "Send with Enter key"},
             {'h', "help", "Show this"}}
        );
        static_assert(args_list.check());
        argument_t args(argc, argv, args_list);
        while (!args.empty()) {
            switch (args.get_opt()) {
                case args_list('a'):
                    addr = args.pop();
                    break;
                case args_list('p'): {
                    const auto tmp = args.pop();
                    std::from_chars(tmp.begin(), tmp.end(), port);
                } break;
                case args_list('r'):
                    rtp_path = args.pop();
                    break;
                case args_list('i'): {
                    const auto tmp = args.pop();
                    std::from_chars(tmp.begin(), tmp.end(), interval);
                } break;
                case args_list('s'): {
                    const auto tmp = args.pop();
                    std::from_chars(tmp.begin(), tmp.end(), skep_frame);
                } break;
                case args_list('f'): {
                    const auto tmp_s = args.pop();
                    int64_t tmp_i;
                    std::from_chars(tmp_s.begin(), tmp_s.end(), tmp_i);
                    allowable_time = static_cast<int64_t>(1.0 / tmp_i * 1000);
                } break;
                case args_list('l'): {
                    const auto tmp = args.pop();
                    std::from_chars(tmp.begin(), tmp.end(), file_loop);
                } break;
                case args_list("Enter"):
                    is_enter = true;
                    break;
                case args_list('h'):
                    args_list.print_arg();
                    exit(0);
                default:
                    fprintf(stderr, "unknown argument: %s\n", args.show().data());
                    exit(1);
            }
        }
    }

    RTPFile rtp(rtp_path.data());
    UDPSender udp(addr.data(), port);
    uint8_t send_buffer[1600] = {};
    uint16_t send_pktsize     = 0;

    size_t now_frame = 0;

    uint32_t pre_sequence_number = 0;

    uint32_t first_seq       = 0;
    uint32_t diff_seq        = 0;
    size_t current_file_loop = 0;

    size_t sended_packet = 0;

    std::string input;
    size_t num_input = 0;

    auto next = std::chrono::system_clock::now(); // 1 ループに 16ms 以上で行うことで 60fps を再現
    while (true) {
        next += std::chrono::milliseconds(allowable_time);

        send_pktsize = rtp.get_packet(send_buffer, diff_seq * current_file_loop); // main packet
        if (send_pktsize == 0) {
            if (current_file_loop == file_loop) {
                break;
            } else {
                if (current_file_loop == 0) diff_seq = pre_sequence_number - first_seq + 1;
                // 2周目以降のための初期設定
                ++current_file_loop;
                rtp.reopen(rtp_path.data());
                send_pktsize = rtp.get_packet(send_buffer, diff_seq * current_file_loop);
            }
        }

        if (num_input != 0) {
            if (--num_input == 0) is_enter = true;
        }
        if (is_enter) {
            if (sended_packet > 1) std::cout << sended_packet << std::endl;
            int c = 0;
            while ((c = getchar()) != '\n') {
                input.push_back(c);
            }
            if (!input.empty()) {
                std::from_chars(input.begin().base(), input.end().base(), num_input);
                is_enter = false;
                input.clear();
            }
            std::cout << "send main packet, number of sent packets: " << sended_packet << " -> ";
            if (num_input == 0 && is_enter == false) std::cout << std::endl;
            udp.send(send_buffer, send_pktsize);
        } else {
            udp.send(send_buffer, send_pktsize);
        }
        sended_packet++;
        now_frame++;

        uint32_t sequence_number = (J2KPayloadHeader(RTPHeader(send_buffer).get_header_length() + send_buffer).get_ESEQ() << 16) | RTPHeader(send_buffer).get_sequence_number();
        if (!((sequence_number == pre_sequence_number + 1) || (pre_sequence_number == 0) || (sequence_number == 0))) {
            std::cout << "assert false sequence_number: " << sequence_number << ", pre_sequence_number: " << pre_sequence_number << std::endl;
            exit(1);
        }
        if (pre_sequence_number == 0) first_seq = sequence_number;
        pre_sequence_number = sequence_number;

        assert(J2KPayloadHeader(RTPHeader(send_buffer).get_header_length() + send_buffer).get_MH());
        while (true) { // body packet
            send_pktsize = rtp.get_packet(send_buffer, diff_seq * current_file_loop);
            if (num_input != 0) {
                if (--num_input == 0) is_enter = true;
            }
            if (is_enter) {
                std::cout << sended_packet << std::endl;
                int c = 0;
                while ((c = getchar()) != '\n') {
                    input.push_back(c);
                }
                if (!input.empty()) {
                    std::from_chars(input.begin().base(), input.end().base(), num_input);
                    is_enter = false;
                    input.clear();
                }
                std::cout << "send body packet, number of set packets: " << sended_packet << " -> ";
                if (num_input == 0 && is_enter == false) std::cout << std::endl;
                udp.send(send_buffer, send_pktsize);
            } else {
                udp.send(send_buffer, send_pktsize);
            }
            sended_packet++;
            // std::this_thread::sleep_for(std::chrono::milliseconds(interval));
            std::this_thread::sleep_for(std::chrono::microseconds(interval));

            uint32_t sequence_number = (J2KPayloadHeader(RTPHeader(send_buffer).get_header_length() + send_buffer).get_ESEQ() << 16) | RTPHeader(send_buffer).get_sequence_number();
            if (!((sequence_number == pre_sequence_number + 1) || (pre_sequence_number == 0) || (sequence_number == 0))) {
                std::cout << "assert false sequence_number: " << sequence_number << ", pre_sequence_number: " << pre_sequence_number << std::endl;
                exit(1);
            }
            pre_sequence_number = sequence_number;

            if (RTPHeader(send_buffer).get_M()) break; // get_M() == true のとき EOF をパケットに含む
        }

        if (now_frame % 10 == 0) {
            printf("frame: %ld\n", now_frame);
        }

        std::this_thread::sleep_until(next);
    }
    char stop = 0;
    udp.send(&stop, 1);
    printf("sended_pakcet: %ld\n", sended_packet);

    return 0;
}