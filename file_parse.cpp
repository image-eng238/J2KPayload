#include "argument.hpp"
#include "fast_table.hpp"
#include "RTPsender2.hpp"

#include <array>
#include <memory>

int main(int argc, char** argv) {
    std::string_view rtp_file;
    {
        using namespace tklib;
        static constexpr argument_list args_list(
            {{'r', "rtp_file", "The .rtp file source of packet to parse"},
             {'h', "help", "Show this"}}
        );
        argument_t args{argc, argv, args_list};
        while (!args.empty()) {
            switch (args.get_opt()) {
                case args_list('r'):
                    rtp_file = args.pop();
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

    RTP_file rtpf(rtp_file.data());
    constexpr size_t PACKET_BUFFER_LENGTH = 1360;
    static leaky_bucket_buf::link_list packet_buffer[PACKET_BUFFER_LENGTH];
    leaky_bucket_buf buffer(nullptr, packet_buffer, PACKET_BUFFER_LENGTH);
    RTPReceiver rtp_recv(&buffer);

    for (size_t i = 0; i < rtpf.num_packet();) {
        {
            packet_t pkt;
            do {
                pkt = rtpf.get_pkt(i);
                buffer.push(pkt.data(), pkt.size());
                ++i;
            } while (!RTPHeader_trait::get_M(pkt.data()));
        }

        MainHeader main_header;
        Tile j2k_tile;
        std::array<fast_table, ConstValue::all_precinct> j2k_packet_table{};
        rtp_recv.first_check();
        J2kBuf buf(&rtp_recv);
        main_header.read(buf);
        j2k_tile.init(main_header, buf);
        j2k_tile.read(main_header, j2k_packet_table);

        uint32_t PID       = 0;
        size_t table_index = 0;
        while (!rtp_recv.EOC()) {
            rtp_recv.check();
            PID = rtp_recv.get_PID();
            while (j2k_packet_table[table_index].PID != PID) {
                j2k_packet_table[table_index].read_packet(buf);
                ++table_index;
            }
        }
    }
    return 0;
}