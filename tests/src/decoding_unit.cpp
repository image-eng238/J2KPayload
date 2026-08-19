#include "argument.hpp"
#include "codestream.hpp"
#include "decoding_unit.hpp"
#include "RTPsender2.hpp"

int main(int argc, char** argv) {
    std::string_view rtp_filepath;
    size_t target_frame  = 1;
    uintmax_t alloc_size = UINTMAX_MAX;
    {
        using namespace tklib;
        static constexpr argument_list opts(
            optspec_t('s', "allocate_size", true, "allocate size for .rtp file"),
            optspec_t{'r', "rtp_file", true, "The .rtp file source of packet to parse"},
            optspec_t{'f', "frame", true, "Specify frames to process"},
            optspec_t{'h', "help", false, "Show this"}
        );
        argument_t args{argc, argv, opts};
        while (args.can_parse()) {
            switch (args.getopt()) {
                case opts('s'):
                    alloc_size = args.get_value<uintmax_t>().value_or(UINTMAX_MAX);
                    break;
                case opts('r'):
                    rtp_filepath = args.get_str();
                    break;
                case opts('f'):
                    if (auto tmp = args.get_value<size_t>(); tmp) target_frame = tmp.value();
                    break;
                case opts('h'):
                    printf("Options: \n%s\n", TKLIB_ARG_DESCRIPTION(opts, dft_style_fmt));
                case opts(opt_err::ambiguous):
                    std::cerr << "'--" << args.get_last_parse() << "' is ambiguous" << std::endl;
                    exit(1);
                case opts(opt_err::no_argument):
                    std::cerr << "'" << args.get_last_parse() << "' requires an argument" << std::endl;
                    exit(1);
                default:
                    std::cerr << "'" << args.get_last_parse() << "' is unknown argument" << std::endl;
                    exit(1);
            }
        }
    }
    RTP_file rtpf;
    tklib::interval_t<size_t> frame_range{};
    if (!rtpf.load(rtp_filepath, alloc_size)) {
        std::cerr << "can't load file: '" << rtp_filepath << "'" << std::endl;
        exit(1);
    }
    if (rtpf.num_frame() + 1 < target_frame) {
        std::cerr << "frame " << target_frame << " is out range" << std::endl;
        exit(1);
    }

    packet_t pkt;
    for (uintmax_t i = 0, frm = 0; i < rtpf.num_packet(); ++i) {
        pkt = rtpf.get_pkt(i);
        if (J2KPayloadHeader_trait::get_MH(pkt.data() + RTPHeader_trait::length)) {
            ++frm;
            if (frm == target_frame) {
                frame_range.first = i;
            }
        }
        frame_range.last = i;
        if (RTPHeader_trait::get_M(pkt.data()) && frm == target_frame) {
            break;
        }
    }

    constexpr size_t PACKET_BUFFER_LENGTH = 1360;
    static leaky_bucket_buf::link_list packet_buffer[PACKET_BUFFER_LENGTH];
    leaky_bucket_buf buffer(nullptr, packet_buffer, PACKET_BUFFER_LENGTH);
    RTPReceiver rtp_recv(&buffer);
    {
        size_t i = frame_range.first;
        do {
            pkt = rtpf.get_pkt(i++);
            buffer.push(pkt.data(), pkt.size());
        } while (i <= frame_range.last);
    }

    MainHeader main_header;
    j2k_Tile j2k_tile;
    rtp_recv.load_main_packet();
    J2kBuf buf(&rtp_recv);
    main_header.read(buf);
    auto* ptr = j2k_tile.resource_ptr();
    ptr->prev_allocate(1836, 0, 0);
    j2k_tile.init(main_header, buf);

    return 0;
}