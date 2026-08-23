#include "argument.hpp"
#include "decoding_unit.hpp"
#include "RTPsender2.hpp"

#include <array>
#include <memory>
#include <charconv>
#include <fstream>

// r: rtp ファイル
// f: 解析するフレームを範囲で指定(first-last)
// m: rtp ファイルの処理方法．rtp ファイル，j2c ファイル，log ファイル
// i: 入力ファイル．rtp ファイル，j2c ファイル を指定
// o: 出力の書き込み先．省略時は標準出力

int main(int argc, char** argv) {
    std::string_view rtp_path, output_path, mode{"log"};
    tklib::interval_t<size_t> inv{};
    {
        using namespace tklib;
        static constexpr argument_list opts(
            optspec_t{'r', "rtp_file", true, "The .rtp file source of packet to parse"},
            optspec_t{'f', "frame", true, "Specify frames to process (first-last)"},
            optspec_t{'m', "mode", true, "Processing mode (j2c, log)"},
            // optspec_t{'i', "input", true, "input file"},
            optspec_t{'o', "output", true, "output file, std if omitted"},
            optspec_t{'h', "help", false, "Show this"}
        );
        argument_t args{argc, argv, opts};
        while (args.can_parse()) {
            switch (args.getopt()) {
                case opts('r'):
                    rtp_path = args.get_str();
                    break;
                case opts('f'):
                    if (auto tmp = args.get_interval<size_t>(); tmp) inv = tmp.value();
                    break;
                case opts('m'):
                    mode = args.get_str();
                    if (mode != "j2c" && mode != "log") {
                        std::cerr << "'" << mode << "' is unknown mode" << std::endl;
                        exit(1);
                    }
                    break;
                case opts('o'):
                    output_path = args.get_str();
                    break;
                case opts('h'):
                    printf("Usage:\n%s\n", TKLIB_ARG_DESCRIPTION(opts, dft_style_fmt));
                    exit(0);
                case opts(opt_err::ambiguous): {
                    std::array<std::string_view, 3> amb;
                    const auto re = opts.get_ambiguous(args.get_last_parse(), amb.begin(), amb.end());
                    std::cerr << "'--" << args.get_last_parse() << "' is ambiguous as ";
                    for (auto it = amb.begin(); it != re; ++it) std::cerr << *it << " ";
                    std::cerr << std::endl;
                    exit(1);
                }
                case opts(opt_err::no_argument): {
                    std::cerr << "'" << args.get_last_parse() << "' requires an argument" << std::endl;
                }
                default:
                    std::cerr << "'" << args.get_last_parse() << "' is unknown argument" << std::endl;
                    exit(1);
            }
        }
    }

    const auto frame_max = inv.last + 1 - inv.first;
    RTP_file rtpf(rtp_path);
    tklib::interval_t<size_t> frame_range{};
    packet_t pkt;
    for (size_t i = 0, frm = 0; i < rtpf.num_packet(); ++i) {
        pkt = rtpf.get_pkt(i);
        if (J2KPayloadHeader_trait::get_MH(pkt.data() + RTPHeader_trait::length)) {
            ++frm;
            if (frm == inv.first) {
                frame_range.first = i;
            }
        }
        if (RTPHeader_trait::get_M(pkt.data())) {
            if (frm == inv.last) {
                frame_range.last = i + 1;
                break;
            }
        }
    }

    if (mode == "log") {
        constexpr size_t PACKET_BUFFER_LENGTH = 1360;
        static leaky_bucket_buf::link_list packet_buffer[PACKET_BUFFER_LENGTH];
        leaky_bucket_buf buffer(nullptr, packet_buffer, PACKET_BUFFER_LENGTH);
        RTPReceiver rtp_recv(&buffer);

        for (size_t i = 0, j = frame_range.first; i < frame_max; ++i) {
            // load 1 frame
            do {
                pkt = rtpf.get_pkt(j++);
                buffer.push(pkt.data(), pkt.size());
            } while (!RTPHeader_trait::get_M(pkt.data()));

            MainHeader main_header;
            j2k_Tile j2k_tile;
            rtp_recv.load_main_packet();
            J2kBuf buf(&rtp_recv);
            main_header.read(buf);
            j2k_tile.init(main_header, buf);

            auto& j2k_packet_table = j2k_tile.acs_table();
            uint32_t PID           = 0;
            size_t table_index     = 0;
            while (true) {
                rtp_recv.load_body_packet();
                PID = rtp_recv.get_PID();
                while (table_index < j2k_packet_table.size() && j2k_packet_table[table_index].get_PID() != PID) {
                    j2k_packet_table[table_index].read_packet(buf);
                    ++table_index;
                }
                if (table_index == j2k_packet_table.size()) {
                    auto m = buf.get_byte(2);
                    assert(m == j2kmk::EOC);
                    rtp_recv.terminate();
                    break;
                }
            }
        }

    } else if (mode == "j2c") {
        if (output_path.empty()) {
            std::cerr << "please specify --output_file" << std::endl;
            exit(1);
        }

        const auto hd = RTPHeader_trait::get_header_length() + J2KPayloadHeader_trait::get_header_length();
        std::fstream output_file;
        output_file.open(output_path.data(), std::ios::out);
        if (!output_file.is_open()) {
            std::cerr << "can't open file: '" << output_path << "'" << std::endl;
            exit(1);
        }

        for (size_t i = frame_range.first; i < frame_range.last; ++i) {
            pkt = rtpf.get_pkt(i);
            output_file.write(reinterpret_cast<const char*>(pkt.data()) + hd, pkt.size() - hd);
        }

    } else {
        exit(1);
    }

    return 0;
}