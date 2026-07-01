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
#include "UDP.hpp"
#include "RTP_header.hpp"

#include <string>
#include <string_view>
#include <charconv>
#include <thread>
#include <chrono>
#include <memory>
#include <filesystem>
#include <cstdio>
#include <vector>
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

template <typename T, typename L = std::size_t>
struct pointer_with_length {
    using value_type             = T;
    using pointer                = value_type*;
    using const_pointer          = const value_type*;
    using reference              = value_type&;
    using const_reference        = const value_type&;
    using iterator               = value_type*;
    using const_iterator         = const value_type*;
    using size_type              = L;
    using difference_type        = std::ptrdiff_t;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    pointer ptr;
    size_type len;

    constexpr reference at(size_type n) {
        if (n >= len) {
            throw std::out_of_range{"pointer_with_lengh::at: n"};
        }
        return ptr[n];
    }
    constexpr const_reference at(size_type n) const {
        if (n >= len) {
            throw std::out_of_range{"pointer_with_lengh::at: n"};
        }
        return ptr[n];
    }

    constexpr reference operator[](size_type n) noexcept { return ptr[n]; }
    constexpr const_reference operator[](size_type n) const noexcept { return ptr[n]; }

    constexpr reference front() noexcept { return ptr[static_cast<size_type>(0)]; }
    constexpr const_reference front() const noexcept { return ptr[static_cast<size_type>(0)]; }

    constexpr reference back() noexcept { return ptr[len - 1]; }
    constexpr const_reference back() const noexcept { return ptr[len - 1]; }

    constexpr pointer data() noexcept { return static_cast<pointer>(ptr); }
    constexpr const_pointer data() const noexcept { return static_cast<const_pointer>(ptr); }

    constexpr iterator begin() noexcept { return iterator(data()); }
    constexpr const_iterator begin() const noexcept { return const_iterator(data()); }

    constexpr iterator end() noexcept { return iterator(data() + len); }
    constexpr const_iterator end() const noexcept { return const_iterator(data() + len); }

    constexpr reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    constexpr const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }

    constexpr reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    constexpr const_reverse_iterator rend() const noexcept { const_reverse_iterator(begin()); }

    constexpr const_iterator cbegin() const noexcept { return const_iterator(data()); }

    constexpr const_iterator cend() const noexcept { return const_iterator(data() + len); }

    constexpr const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(end()); }

    constexpr const_reverse_iterator crend() const noexcept { return const_reverse_iterator(begin()); }

    constexpr size_type size() const noexcept { return len; }
    constexpr size_type max_size() const noexcept { return len; }
    constexpr bool empty() const noexcept { return size() == 0; }
};
using packet_t = pointer_with_length<uint8_t>;

class RTP_file {
public:
    RTP_file() : codestream{}, packets{}, siz{} {};
    RTP_file(const char* file_path) : RTP_file{} {
        if (!load(file_path)) {
            fprintf(stderr, "can`t load file: '%s'\n", file_path);
            exit(1);
        }
    }
    bool load(const char* f) {
        siz        = std::filesystem::file_size(f);
        codestream = std::make_unique<uint8_t[]>(siz);
        FILE* fp   = fopen(f, "r");
        if (fp == nullptr) {
            return false;
        }
        const auto s = fread(codestream.get(), sizeof(uint8_t), siz, fp);
        fclose(fp);
        if (s != siz) {
            return false;
        }

        const size_t hd = 4;
        for (uintmax_t i = 0; i != siz;) {
            uint8_t* const pktdata = codestream.get() + i;
            if (pktdata[0] != 0xFF || pktdata[1] != 0xFF) {
                return false;
            }
            const size_t pktsiz = (pktdata[2] << 8) | pktdata[3];
            packets.push_back(packet_t{pktdata + hd, pktsiz});
            i += pktsiz + hd;
        }
        return true;
    }
    auto get_pkt(size_t i) const { return packets[i]; }
    auto& front() { return packets.front(); }
    auto& back() { return packets.back(); }
    auto num_packet() const { return packets.size(); }

private:
    std::unique_ptr<uint8_t[]> codestream;
    std::vector<packet_t> packets;
    uintmax_t siz;
};

class cli_parser {
public:
    cli_parser() : ignore_count{}, input{}, opt_number{}, opt_character{}, is_active{false} {}
    cli_parser(bool active) : cli_parser{} { is_active = active; }

    bool read_line() {
        if (!is_active) return false;

        if (ignore_count != 0) {
            --ignore_count;
            return false;
        }

        printf("> ");
        int c = 0;
        input.clear();
        while ((c = getchar()) != '\n') {
            if (c == -1) { return false; }
            input.push_back(c);
        }
        if (input.empty()) {
            opt_character = 's';
            opt_number    = 1;
            return true;
        }

        while (true) {
            try {
                opt_character     = input.at(input.find_first_not_of(' '));
                const auto numpos = input.find_first_of("0123456789", 0);
                if (numpos == 0) {
                    opt_character = 's';
                }
                if (numpos != std::string::npos) {
                    std::string_view str{input.data() + numpos};
                    opt_number = std::stoi(std::string{str});
                } else {
                    opt_number = 1;
                }
                return true;
            } catch (std::runtime_error& e) {
                std::cerr << e.what();
            } catch (std::exception& e) {
                std::cerr << e.what();
                exit(1);
            }
        }
    }

    void set_ignore(size_t n) { ignore_count = n - 1; }

    int optn() const { return opt_number; }
    char optc() const { return opt_character; }
    bool active() const { return is_active; }

private:
    size_t ignore_count;
    std::string input;
    int opt_number;
    char opt_character;
    bool is_active;
};

class packet_os {
public:
    packet_os() : tp{}, base_tp{}, exseq{}, base_exseq{}, ptp{}, base_ptp{} {};
    packet_os(const packet_t& pkt) : packet_os{} { set_base(pkt); }

    void set_base(const packet_t& pkt) {
        tp = base_tp = RTPHeader_trait::get_timestamp(pkt.data());
        exseq = base_exseq = J2KPayloadHeader_trait::get_extended_sequence_number(pkt.data());
        ptp = base_ptp = J2KPayloadHeader_trait::get_PTSTAMP(pkt.data() + RTPHeader_trait::length);
    }

    void advance_tp(packet_t& pkt, uint32_t n) {
        RTPHeader_trait::set_timestamp(pkt.data(), tp);
        if (RTPHeader_trait::get_M(pkt.data())) {
            tp = tp.get() + n;
        }
    }
    void advance_seq(packet_t& pkt) { J2KPayloadHeader_trait::set_extended_sequence_number(pkt.data(), exseq++); }
    void advance_ptp(packet_t& pkt, uint16_t n) {
        J2KPayloadHeader_trait::set_PTSTAMP(pkt.data() + RTPHeader_trait::length, ptp);
        ptp = ptp.get() + n;
    }

    void set_tp(uint32_t n) { tp = n; }
    void set_exseq(uint32_t n) { exseq = n; }
    void set_ptp(uint32_t n) { ptp = n; }

    void write_tp(packet_t& pkt) { RTPHeader_trait::set_timestamp(pkt.data(), tp); }
    void write_exseq(packet_t& pkt) { J2KPayloadHeader_trait::set_extended_sequence_number(pkt.data(), exseq); }
    void write_ptp(packet_t& pkt) { J2KPayloadHeader_trait::set_PTSTAMP(pkt.data() + RTPHeader_trait::length, ptp); }
    void write(packet_t& pkt) {
        write_tp(pkt);
        write_exseq(pkt);
        write_ptp(pkt);
    }

private:
    rtptimestamp_t tp;
    uint32_t base_tp;
    exsequence_t exseq;
    uint32_t base_exseq;
    j2kptstamp_t ptp;
    uint16_t base_ptp;
};

class packet_sender {
public:
    packet_sender(std::string_view addr, uint16_t port)
        : udp{addr.data(), port}, send_call{}, lost_packet{}, ignore_count{}, sent_frame{}, out_frame{}, sum_avg{}, data_fps{}, adv_sleep_v{}, sleep_v{} {}

    bool send(const packet_t& pkt) {
        ++send_call;
        if (ignore_count != 0) {
            --ignore_count;
            ++lost_packet;
        } else {
            if (udp.send(pkt.ptr, pkt.len) == -1) {
                perror("sendto");
                return false;
            }
        }
        if (RTPHeader_trait::get_M(pkt.data())) {
            ++sent_frame;
            std::this_thread::sleep_until(sleep_v += adv_sleep_v);
            if (sent_frame % out_frame == 0) {
                const auto now_time = std::chrono::steady_clock::now();
                const auto ms       = std::chrono::duration_cast<std::chrono::microseconds>(now_time - prev_time);
                const auto fps      = (1 / (static_cast<float>(ms.count()) / out_frame)) * 1000000;
                sum_avg += fps;
                printf("sent_frame: %ld, avg: %.6ffps\n", sent_frame, fps);
                prev_time = now_time;
            }
        }
        return true;
    }

    void set_ignore(size_t n) { ignore_count = n; }
    void set_clock(size_t t) {
        data_fps    = static_cast<double>(J2KPayloadHeader_trait::media_clock_Hz) / t;
        adv_sleep_v = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(static_cast<double>(t) / J2KPayloadHeader_trait::media_clock_Hz)
        );
        out_frame = static_cast<size_t>(J2KPayloadHeader_trait::media_clock_Hz / static_cast<double>(t) + 0.5);
        prev_time = sleep_v = std::chrono::steady_clock::now();
    }
    void set_sleep_v() { prev_time = sleep_v = std::chrono::steady_clock::now(); }

    size_t get_call() const { return send_call; }

    void print_result() {
        printf("========================================\n");
        printf("fps based on data: %lffps ~= %ldfps\n", data_fps, out_frame);
        printf("average fps: %lffps\n", sum_avg / (sent_frame / out_frame));
        printf("lost packets: %ld\n", lost_packet);
        printf("sent packets: %ld\n", send_call - lost_packet);
        printf("sent frames: %ld\n", sent_frame);
    }

private:
    UDPSender udp;
    size_t send_call;
    size_t lost_packet;
    size_t ignore_count;
    size_t sent_frame;
    size_t out_frame;
    double sum_avg;
    double data_fps;
    std::chrono::steady_clock::duration adv_sleep_v;
    std::chrono::steady_clock::time_point sleep_v;
    std::chrono::steady_clock::time_point prev_time;
};

int main(int argc, char** argv) {
    std::string_view addr = "127.0.0.1", rtp_path;
    uint16_t port         = 50001;
    size_t fps_overwrite  = SIZE_MAX;
    size_t number_of_loop = 1;
    bool is_enter_opt     = false;

    std::chrono::steady_clock interval                      = {};
    std::chrono::steady_clock::time_point allowable_time_ms = {};

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
                    // std::from_chars(tmp.begin(), tmp.end(), interval);
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
    for (size_t i = 0; i < number_of_loop; ++i) {
        for (size_t p = 0; p < rtpfile.num_packet();) {
            auto pkt = rtpfile.get_pkt(p);
            if (cli.read_line()) {
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
                            fprintf(fp, "packet[%ld] <-\nframe = %ld\nsize = %ld\n", num_pkt % packet_in_frame + 1, num_pkt / packet_in_frame + 1, pkt.size());
                            RTPHeader_trait::print_info(fp, pkt.data());
                            J2KPayloadHeader_trait::print_info(fp, pkt.data());
                            putc('\n', fp);
                            for (size_t iS = 1; iS < cli.optn(); ++iS) {
                                const auto pos      = (++num_pkt < rtpfile.num_packet()) ? num_pkt : 0 + iS;
                                const packet_t pktS = rtpfile.get_pkt(pos);
                                fprintf(fp, "packet[%ld]\nframe = %ld\nsize = %ld\n", pos % packet_in_frame + 1, pos / packet_in_frame + 1, pktS.size());
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
                        size_t ce;
                        for (ce = 0;
                             !RTPHeader_trait::get_M(rtpfile.get_pkt(p + ce).data());
                             ++ce);
                        cli.set_ignore(1 + ce + packet_in_frame * (cli.optn() - 1));
                    } break;
                    case 'h': { // help
                        print2pager([](FILE* fp) {
                            fprintf(fp, "s: send n packets\n");
                            fprintf(fp, "l: loss n packets\n");
                            fprintf(fp, "c: change sequence to n\n");
                            fprintf(fp, "v: view the data of n pakcets\n");
                            fprintf(fp, "r: send to rsync point\n");
                            fprintf(fp, "e: send to EOC\n");
                            fprintf(fp, "h: help\n");
                            fprintf(fp, "q: quit\n");
                        });
                        continue;
                    }
                    case 'q': // exit
                        goto EndOfLoop;
                    default:
                        fprintf(stderr, "unknown argument: '%c', code: %d\n", c, c);
                        continue;
                }
                udp.set_sleep_v();
            }
            if (sig_flag) {
                putc('\n', stdout);
                goto EndOfLoop;
            }

            pktos.advance_tp(pkt, adv_tp_v);
            pktos.advance_seq(pkt);
            if (!udp.send(pkt)) {
                fprintf(stderr, "send error\n");
                exit(1);
            }
            ++p;
        }
    }
EndOfLoop:

    udp.print_result();

    return 0;
}