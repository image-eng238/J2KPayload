// ./build/bin/RTPsender -r ./data/2160p_5994fps_422_10bit.rtp -a 127.0.0.1 -p 50001 -s 1 -i 1

// 2160p_5994fps_422_10bit.rtp パケット数は 163200 2秒分のサンプルなので 1フレーム当たりのパケット数は 1360

/*
コマンドライン引数は従来通り．
--Enter オプションでの挙動を変更，実装の容易性のため簡易的な入力にする．
format: <command> <number>
<command> と <number> の間はスペースをで区切る．
<command> によっては <number> を無視．
<number> に入力された数値を <command> に渡す．空は 1, 0 以下は無効．
無効な <command>, <number> が渡された場合は何もしない．

--- <command> に渡す値 ---
<number> optional
s: send，パケットを送信
l: loss, パケットを破棄
c: change, シーケンス番号を変更，送信をしない
v: view, 次に送るパケットのデータを確認，送信をしない
r: rsync, 次の再同期ポイントまで送信
e: EOC, EOCが出現するまで送信

<number> unnecessary
R: restart, 再起動
h: help, ヘルプ
q: quit, 終了
*/

/*
必要な処理
パケットの送信処理
パケットの情報を書き換える処理
パケットを破棄する処理
--Enter オプションでのコマンド解析処理
*/

#include "argument.hpp"
#include "RTPsender2.hpp"

#include <string>
#include <string_view>
#include <charconv>
#include <chrono>
#include <memory>
#include <cstdio>
#include <csignal>

sig_atomic_t sig_flag = 0;
void sig_handler(int sig_num) {
    sig_flag = 1;
}

template <typename Callback>
void print2pager(Callback print_function) {
    std::string_view pager = getenv("PAGER");
    if (pager.empty()) {
        pager = "less";
    }
    FILE* fp = popen(pager.data(), "w");
    if (fp == nullptr) {
        perror("popen");
        exit(1);
    }
    print_function(fp);
    if (pclose(fp) == -1) {
        perror("pclose");
        exit(1);
    }
}

int main(int argc, char** argv) {
    std::string_view addr = "127.0.0.1", rtp_path;
    uint16_t port         = 50001;
    size_t fps_overwrite  = SIZE_MAX;
    size_t number_of_loop = 1;
    bool is_enter_opt     = false;

    std::chrono::steady_clock::duration interval{};

    {
        using namespace tklib;
        static constexpr argument_list args_list(
            {{'a', "address", "Send to IPv4 address. default: 127.0.0.1"},
             {'p', "port", "Send to port. default: 50001"},
             {'r', "rtp_file", "The .rtp file source of packet to send"},
             {'i', "interval", "Packet transmission interval (microseconds)"},
             {'f', "frame_rate", "Overwrite the frame fate. default: values in the rtp file"},
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
                    double tmpd;
                    std::from_chars(tmp.begin(), tmp.end(), tmpd);
                    interval = std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double, std::micro>(tmpd));
                } break;
                case args_list('f'): {
                    const auto tmp_s = args.pop();
                    std::from_chars(tmp_s.begin(), tmp_s.end(), fps_overwrite);
                    if (fps_overwrite != 0 && fps_overwrite <= J2KPayloadHeader_trait::media_clock_Hz) {
                        fps_overwrite = J2KPayloadHeader_trait::media_clock_Hz / fps_overwrite;
                    } else {
                        fps_overwrite = SIZE_MAX;
                    }
                } break;
                case args_list('l'): {
                    const auto tmp = args.pop();
                    std::from_chars(tmp.begin(), tmp.end(), number_of_loop);
                } break;
                case args_list("Enter"):
                    is_enter_opt = true;
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

    RTP_file rtpfile{rtp_path.data()};
    cli_parser cli{is_enter_opt};
    packet_os pktos{rtpfile.front()};
    packet_sender udp{addr, port};

    const size_t packet_in_frame = [&] {
        size_t n = 0;
        while (!RTPHeader_trait::get_M(rtpfile.get_pkt(n++).data()));
        return n;
    }();
    const auto adv_tp_v = (fps_overwrite != SIZE_MAX)
                              ? fps_overwrite
                              : RTPHeader_trait::get_timestamp(rtpfile.get_pkt(packet_in_frame).data()) - RTPHeader_trait::get_timestamp(rtpfile.front().data());

    struct sigaction sa{};
    sigemptyset(&sa.sa_mask);
    sa.sa_flags   = 0;
    sa.sa_handler = sig_handler;
    if (sigaction(SIGINT, &sa, nullptr) == -1) {
        perror("sigaction(SIGINT)");
        exit(1);
    }

    sigset_t new_sig_set;
    sigemptyset(&new_sig_set);
    sigaddset(&new_sig_set, SIGINT);
    if (sigprocmask(SIG_UNBLOCK, &new_sig_set, nullptr) == -1) {
        perror("sigprocmask");
        exit(1);
    }

    udp.set_clock(adv_tp_v);
RestartTheLoop:
    for (size_t i = 0; i < number_of_loop; ++i) {
        for (size_t p = 0; p < rtpfile.num_packet();) {
            auto pkt = rtpfile.get_pkt(p);
            if (cli.read_line(std::to_string(udp.get_call()).data())) {
                switch (auto c = cli.optc(); c) {
                    case 's': // send
                        cli.set_ignore(cli.optn());
                        break;
                    case 'l': // loss
                        cli.set_ignore(cli.optn());
                        udp.set_ignore(cli.optn());
                        break;
                    case 'c': // change
                        pktos.set_exseq(cli.optn());
                        continue;
                    case 'v': { // view
                        pktos.write(pkt);
                        print2pager([&](FILE* fp) {
                            size_t num_pkt = udp.get_call();
                            fprintf(fp, "packet[%ld] <- next send\nframe = %ld\nsize = %ld\n", udp.get_fpkt() + 1, udp.get_sent_frame() + 1, pkt.size());
                            RTPHeader_trait::print_info(fp, pkt.data());
                            J2KPayloadHeader_trait::print_info(fp, pkt.data());
                            putc('\n', fp);
                            for (size_t iS = 1; iS < cli.optn(); ++iS) {
                                const auto pos      = (++num_pkt < rtpfile.num_packet()) ? num_pkt : 0 + iS;
                                const packet_t pktS = rtpfile.get_pkt(pos);
                                fprintf(fp, "packet[%ld]\nframe = %ld\nsize = %ld\n", udp.get_fpkt() + 1 + iS, udp.get_sent_frame() + 1, pktS.size());
                                RTPHeader_trait::print_info(fp, pktS.data());
                                J2KPayloadHeader_trait::print_info(fp, pktS.data());
                                putc('\n', fp);
                            }
                        });
                        continue;
                    }
                    case 'r': { // rsync
                        size_t cr = 0;
                        for (size_t i = 0; i < cli.optn(); ++i, ++cr) {
                            for (; !J2KPayloadHeader_trait::get_body_ORDB(rtpfile.get_pkt(p + cr).data() + RTPHeader_trait::length);
                                 ++cr);
                        }
                        cli.set_ignore(cr);
                    } break;
                    case 'e': { // EOC
                        size_t ce = 0;
                        for (size_t i = 0; i < cli.optn(); ++i, ++ce)
                            for (; !RTPHeader_trait::get_M(rtpfile.get_pkt(p + ce).data()); ++ce);
                        cli.set_ignore(ce);
                    } break;
                    case 'R':
                        pktos.to_base();
                        udp.print_result();
                        putc('\n', stdout);
                        udp.clear();
                        udp.set_sleep_v();
                        goto RestartTheLoop;
                    case 'h': { // help
                        print2pager([](FILE* fp) {
                            fprintf(fp, "s: send n packets\n");
                            fprintf(fp, "l: loss n packets\n");
                            fprintf(fp, "c: change sequence to n\n");
                            fprintf(fp, "v: view the data of n pakcets\n");
                            fprintf(fp, "r: send to rsync point\n");
                            fprintf(fp, "e: send to EOC\n");
                            fprintf(fp, "R: restart\n");
                            fprintf(fp, "h: help\n");
                            fprintf(fp, "q: quit\n");
                        });
                        continue;
                    }
                    case 'q': // exit
                        goto EndTheLoop;
                    default:
                        fprintf(stderr, "unknown argument: '%c', code: %d\n", c, c);
                        continue;
                }
                udp.set_sleep_v();
            }
            if (sig_flag) {
                putc('\n', stdout);
                goto EndTheLoop;
            }

            pktos.advance_tp(pkt, adv_tp_v);
            pktos.advance_seq(pkt);
            if (!udp.send(pkt)) {
                fprintf(stderr, "send error\n");
                exit(1);
            }
            ++p;
            std::this_thread::sleep_for(interval);
        }
    }
EndTheLoop:

    udp.print_result();

    return 0;
}