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
<number> required
s: send，パケットを送信
l: loss, パケットを破棄
c: change, シーケンス番号を変更

<number> unnecessary
S: Show, 次に送るパケットのデータを確認
r: rsync, 次の再同期ポイントまで送信
E: EOC, EOCが出現するまで送信
e: exit, 終了
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

class RTP_file {
public:
    RTP_file() : codestream{}, siz{}, pos{} {};
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
        return true;
    }
    size_t get_pkt(uint8_t*& ptr) {
        const size_t hd  = 4;
        uint8_t* pktdata = codestream.get() + pos;
        if (pktdata[0] == 0xFF && pktdata[1] == 0xFF) {
            const size_t pktsiz = (pktdata[2] << 8) | (pktdata[3]);

            ptr = pktdata + hd;
            pos += pktsiz + hd;
            return pktsiz;
        } else if (pktdata[0] == 0 && pktdata[1] == 0) {
            pktdata = nullptr;
            return 0;
        }
        fprintf(stderr, "file format error\n");
        exit(1);
    }
    void restart() { pos = 0; }
    bool EOC() const { return siz == pos; }

private:
    std::unique_ptr<uint8_t[]> codestream;
    uintmax_t siz;
    size_t pos;
};

class cli_parser {
public:
    cli_parser() : input{}, opt_number{}, opt_character{}, is_active{false} {}
    cli_parser(bool active) : cli_parser{} { is_active = active; }

    bool get() {
        if (!is_active) return false;
        printf("> ");
        int c = 0;
        input.clear();
        while ((c = getchar()) != '\n') {
            input.push_back(c);
        }

        while (true) {
            try {
                opt_character     = input.at(input.find_first_not_of(' '));
                const auto numpos = input.find_first_of("0123456789", 0);
                if (numpos == 0) {
                    opt_character = 's';
                }
                std::string_view str{input.data() + numpos};
                std::stoi(std::string{str});
                return true;
            } catch (std::runtime_error& e) {
                std::cerr << e.what();
            } catch (std::exception& e) {
                std::cerr << e.what();
                exit(1);
            }
        }
    }

private:
    std::string input;
    int opt_number;
    char opt_character;
    bool is_active;
};

class packet_sender {
};

class packet_os {
};

int main(int argc, char** argv) {
    std::string_view addr = "127.0.0.1", rtp_path;
    uint16_t port         = 50001;
    size_t skep_frame     = 0;
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
                    // std::from_chars(tmp.begin(), tmp.end(), interval);
                } break;
                case args_list('s'): {
                    const auto tmp = args.pop();
                    std::from_chars(tmp.begin(), tmp.end(), skep_frame);
                } break;
                case args_list('f'): {
                    const auto tmp_s = args.pop();
                    int64_t tmp_i;
                    std::from_chars(tmp_s.begin(), tmp_s.end(), tmp_i);
                    // allowable_time = static_cast<int64_t>(1.0 / tmp_i * 1000);
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

    UDPSender udp{addr.data(), port};

    uint8_t* data_ptr = nullptr;
    size_t data_siz   = 0;

    for (size_t i = 0; i < number_of_loop + 3; ++i) {
        if (cli.get()) {
        } else {
        }
    }

    return 0;
}