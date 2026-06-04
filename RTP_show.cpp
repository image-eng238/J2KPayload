#include "RTP_file_format.hpp"
#include "RTP_header.hpp"
#include "argument.hpp"

#include <string_view>
#include <cstdio>
#include <charconv>

int main(int argc, char** argv) {
    std::string_view rtp_path;
    bool is_enter      = false;
    size_t max_flame   = SIZE_MAX;
    size_t picup_frame = 0;

    {
        using namespace tklib;
        static constexpr argument_list args_list(
            {{'n', "num_frame", "Number of print info for frame"},
             {'p', "picup", "Picup frame"},
             {'r', "rtp_file", "The .rtp file source of packet to send"},
             {0, "Enter", "Print with Enter key"},
             {'h', "help", "Show this"}}
        );
        static_assert(args_list.check());
        argument_t args(argc, argv, args_list);
        while (!args.empty()) {
            switch (args.get_opt()) {
                case args_list('n'): {
                    const auto tmp = args.pop();
                    std::from_chars(tmp.begin(), tmp.end(), max_flame);
                } break;
                case args_list('p'): {
                    const auto tmp = args.pop();
                    std::from_chars(tmp.begin(), tmp.end(), picup_frame);
                } break;
                case args_list('r'):
                    rtp_path = args.pop();
                    break;
                case args_list("Enter"):
                    is_enter = true;
                    break;
                case args_list('h'):
                    args_list.print_arg();
                    exit(0);
                default:
                    fprintf(stderr, "unknown argument: %s\n", args.show().data());
                    exit(1);
                    break;
            }
        }
    }

    uint16_t pktsize     = 0;
    uint8_t pktbuf[1500] = {};
    size_t num_packet    = 0;
    size_t num_frame     = 0;
    bool is_main_packet  = false;
    RTPFile rtp(rtp_path.data());
    const auto hd = RTPHeader_trait::get_header_length();

    auto print_info = [&]() {
        printf("pakcet[%ld]\n", num_packet);
        printf("packet size = %d\n", pktsize);
        RTPHeader_trait::print_info(pktbuf);
        J2KPayloadHeader_trait::print_info(pktbuf);
    };

    while (true) {
        ++num_frame;
        ++num_packet;

        pktsize = rtp.get_packet(pktbuf);
        if (num_frame == picup_frame || picup_frame == 0) {
            printf("frame[%ld]\n", num_frame);
            print_info();
            if (is_enter) {
                int c = fgetc(stdin);
            } else {
                putchar('\n');
            }
        }
        while (true) {
            ++num_packet;
            pktsize = rtp.get_packet(pktbuf);
            if (num_frame == picup_frame || picup_frame == 0) {
                print_info();
                if (is_enter) {
                    int c = fgetc(stdin);
                } else {
                    putchar('\n');
                }
            }
            if (RTPHeader_trait::get_M(pktbuf)) break;
        }
        if (num_frame == picup_frame) break;
        if (max_flame <= num_frame) break;
    }

    return 0;
}