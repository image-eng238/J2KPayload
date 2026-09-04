#include "UDP.hpp"
#include "RTP_header.hpp"
#include "argument.hpp"
#include "measure.hpp"

#include <ratio>
#include <chrono>
#include <cmath>

int main(int argc, char* argv[]) {
    std::string_view addr = "127.0.0.1";
    uint16_t port         = 50001;
    {
        using namespace tklib;
        constexpr argument_list opts(
            optspec_t{'a', "address", true, "IPv4 address default: 127.0.0.1"},
            optspec_t{'p', "port", true, "Port default: 50001"},
            optspec_t{'h', "help", false, "Show this"}
        );
        argument_t args{argc, argv, opts};
        while (args.can_parse()) {
            switch (args.getopt()) {
                case opts('a'):
                    addr = args.get_str();
                    break;
                case opts('p'):
                    port = args.get_value<uint16_t>().value_or(50001);
                    break;
                case opts('h'):
                    printf("Options:\n%s\n", TKLIB_ARG_DESCRIPTION(opts, dft_style_fmt));
                    exit(0);
                default:
                    std::cerr << "'" << args.get_last_parse() << "' is unknown argument" << std::endl;
                    exit(1);
            }
        }
    }
    UDPReceiver udp;
    udp.sock_bind(addr.data(), port);

    std::uint8_t buffer[1500];
    // udp.receive(buffer, 1500);
    // auto now = std::chrono::high_resolution_clock::now().time_since_epoch();
    // auto t   = udp.get_timestamp();

    // auto rtp_tp      = RTPHeader_trait::get_timestamp(buffer);
    // auto to_duration = [](const auto t) constexpr { return std::chrono::duration_cast<std::chrono::high_resolution_clock::duration>(std::chrono::duration<double>{t * (1.0 / J2KPayloadHeader_trait::media_clock_Hz)}); };

    // auto tt        = std::chrono::high_resolution_clock::duration(std::chrono::seconds(t.tv_sec) + std::chrono::nanoseconds(t.tv_nsec));
    // auto test      = tt.count() * J2KPayloadHeader_trait::media_clock_Hz;
    // uint32_t test2 = test;

    // auto Tr = RTPHeader_trait::get_timestamp(buffer);
    // auto Tl = std::chrono::high_resolution_clock::duration(std::chrono::seconds(t.tv_sec) + std::chrono::nanoseconds(t.tv_nsec)).count();

    // auto Tr2Tl = []() {};

    auto Tl2Tr = [](const timespec& t) {
        using namespace J2KPayloadHeader_trait;
        auto Tl_ns = std::chrono::high_resolution_clock::duration(std::chrono::seconds(t.tv_sec) + std::chrono::nanoseconds(t.tv_nsec)).count();
        // auto tmp   = (Tl_ns / media_clock_kHz) / std::ratio_multiply<std::nano, std::kilo>::den;
        // auto tmp   = (Tl_ns / media_clock_kHz) * std::pow(10, -6);
        auto tmp   = (Tl_ns * media_clock_kHz) / std::ratio_multiply<std::nano, std::kilo>::den;
        auto Tr    = static_cast<uint32_t>(tmp);
        return Tr;
    };
    // auto offset = Tl2Tr(t) - Tr;

    // c[kHz] = 90 (media clock rate)
    // Tl[ns] = Tr * (1/c[kHz])
    // rt = ns[ns] * c[kHz]
    // Tl[ns] = Tr * (1 / c[kHz])
    // (Tl * 10^-9) = Tr / (c * 10^3)
    // Tr = Tl[ns] / (1 / c[Khz])
    // Tr = (Tl * 10^-9) / (1 / (c * 10^3) )
    // Tr = (Tl * 10^-9) / (c * 10^-3)
    // Tr = Tl * 10^-9 * 1/c * 1/10^-3
    // Tr = Tl * 10^-9 * 1/c * 10^3
    // Tr = Tl/c * 10^-9 * 10^3
    // Tr = (Tl / c) * 10^-6

    // Tl[ns] = Tr * (1 / c[kHz])
    // Tl[ns] * c[kHz] = Tr * (1 / c[kHz]) * c[kHz]
    // Tl[ns] * c[kHz] = Tr
    // Tr = Tl[ns] * c[kHz]
    // Tr = (Tl[s] * c[Hz]) * (10^-9 * 10^3)
    // Tr = Tl * c * 10^6

    // auto rtp_time = to_duration(rtp_tp);

    // auto offset     = (tt - rtp_time).count();
    // int32_t offset2 = offset;

    // std::cout << "sec=" << t.tv_sec << " nsec=" << t.tv_nsec << " join=" << t.tv_sec << t.tv_nsec << std::endl;
    // std::cout << "std::chrono::high_resolution_clock=" << now.count() << std::endl;

    // auto of = udp.get_overflow_packet();

    j2k_stats<size_t> time_stats;
    j2k_stats<double> result_stddev;
    j2k_stats<double> result_average;
    size_t i = 0;
    while (true) {
        using namespace RTPHeader_trait;
        using namespace J2KPayloadHeader_trait;
        auto siz = udp.receive(buffer, 1500);
        if (siz == 1) { break; }

        if (get_M(buffer)) {
            auto Tr     = get_timestamp(buffer);
            auto Tl     = Tl2Tr(udp.get_timestamp());
            auto offset = Tl - Tr;
            time_stats.add_sample(offset);
            // printf("offset=%u\n", offset);

            // if (time_stats.sample_size() <= 1360) {
            //     ++i;
            //     if (!time_stats.zero_variance()) {
            //         printf(
            //             "index=%zu TP=%u siz=%zu min=%zu max=%zu avg=%f std=%f\n",
            //             i, Tr, time_stats.sample_size(), time_stats.minimum(), time_stats.maximum(),
            //             time_stats.average(), time_stats.stddev()
            //         );
            //     }
            //     if (!time_stats.zero_variance()) {
            //         result_stddev.add_sample(time_stats.stddev());
            //         result_average.add_sample(time_stats.average());
            //     }
            // } else {
            //     time_stats = {};
            // }
            // time_stats = {};
            printf(
                "index=%zu TP=%u siz=%zu min=%zu max=%zu avg=%f std=%f\n",
                ++i, Tr, time_stats.sample_size(), time_stats.minimum(), time_stats.maximum(),
                time_stats.average(), time_stats.stddev()
            );
            result_stddev.add_sample(time_stats.stddev());
            result_average.add_sample(time_stats.average());
        }
    }

    printf("AVG min=%f max=%f avg=%f std=%f\n", result_average.minimum(), result_average.maximum(), result_average.average(), result_average.stddev());
    printf("STD min=%f max=%f avg=%f std=%f\n", result_stddev.minimum(), result_stddev.maximum(), result_stddev.average(), result_stddev.stddev());

    return 0;
}