#include "argument.hpp"
#include "RTPsender2.hpp"

#include <string_view>
#include <cstdio>
#include <iostream>

int main(int argc, char** argv) {
    std::string_view rtp_path;
    bool is_csv       = false;
    bool is_csv_print = false;
    tklib::interval_t<size_t> inv{};

    {
        using namespace tklib;
        static constexpr argument_list opts(
            optspec_t{'f', "frame", true, "Number of print info for frame"},
            optspec_t{'r', "rtp_file", true, "The .rtp file source of packet to send"},
            optspec_t{0, "csv", false, "stdout csv style"},
            optspec_t{0, "csv_fmt", false, "Print csv format"},
            optspec_t{'h', "help", false, "Show this"}
        );
        argument_t args(argc, argv, opts);
        while (args.can_parse()) {
            switch (args.getopt()) {
                case opts('f'):
                    if (auto tmp = args.get_interval<size_t>(); tmp) inv = tmp.value();
                    break;
                case opts('r'):
                    rtp_path = args.get_str();
                    break;
                case opts("csv"):
                    is_csv = true;
                    break;
                case opts("csv_fmt"):
                    is_csv_print = true;
                    break;
                case opts('h'):
                    printf("Usage:\n%s\n", TKLIB_ARG_DESCRIPTION(opts, dft_style_fmt));
                    exit(0);
                case opts(opt_err::ambiguous): {
                    std::array<std::string_view, 3> amb;
                    const auto re = opts.get_ambiguous(args.get_last_parse(), amb.begin(), amb.end());
                    std::cerr << "'--" << args.get_last_parse() << "' is ambiguous as ";
                    for (auto it = amb.begin(); it != re; ++it) std::cerr << it;
                    std::cerr << std::endl;
                    exit(1);
                }
                case opts(opt_err::no_argument): {
                    std::cout << "'" << args.get_last_parse() << "' requires an argument" << std::endl;
                }
                default:
                    std::cout << "'" << args.get_last_parse() << "' is unknown argument" << std::endl;
                    exit(1);
            }
        }
    }
    if (is_csv_print) {
        auto seq = J2KPayloadHeader_trait::print_csv_fmt(stdout);
        printf(",size[%d],frame[%d]\n", seq + 1, seq + 2);
        if (!is_csv) exit(0);
    }

    tklib::interval_t<size_t> frame_range{};

    RTP_file rf{rtp_path};
    packet_t pkt;
    size_t num_frame = 0, num_packet = 0;
    for (size_t i = 0, frm = 0; i < rf.num_packet(); ++i) {
        pkt = rf.get_pkt(i);
        if (J2KPayloadHeader_trait::get_MH(pkt.data() + RTPHeader_trait::length)) {
            ++frm;
            if (frm == inv.first) {
                frame_range.first = i;
                num_frame         = frm - 1;
                num_packet        = i + 1;
            }
        }
        if (RTPHeader_trait::get_M(pkt.data())) {
            if (frm == inv.last) {
                frame_range.last = i + 1;
                break;
            }
        }
    }

    auto print_info = [&]() {
        printf("pakcet[%ld]\n", num_packet);
        printf("packet size = %ld\n", pkt.size());
        RTPHeader_trait::print_info(stdout, pkt.data());
        J2KPayloadHeader_trait::print_info(stdout, pkt.data());
        putc('\n', stdout);
    };

    auto print_csv = [&] {
        J2KPayloadHeader_trait::print_csv(stdout, pkt.data());
        printf(",%d,%ld\n", static_cast<int>(pkt.size()), num_frame);
    };

    for (size_t i = frame_range.first; i < frame_range.last; ++i) {
        pkt = rf.get_pkt(i);
        if (J2KPayloadHeader_trait::get_MH(pkt.data() + RTPHeader_trait::length)) {
            ++num_frame;
        }
        if (is_csv) {
            print_csv();
        } else {
            print_info();
            ++num_packet;
        }
    }
    return 0;
}