#include <array>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>

namespace tklib {
    template <size_t N = 0, int UnknownCode = -1, int NoArugmentCode = -2>
    class argument_list {
    private:
        struct arg_ele {
            int short_opt;
            std::string_view long_opt;
            std::string_view explanation;
            constexpr int opt_type() const {
                auto is_opt = [](const int c) -> bool {
                    if ('0' <= c && c <= '9') return true;
                    if ('A' <= c && c <= 'Z') return true;
                    if ('a' <= c && c <= 'z') return true;
                    return false;
                };
                int outout = 0;
                outout += is_opt(short_opt) ? 1 : 0;
                outout += long_opt.empty() ? 0 : 2;
                return outout;
            }
        };
        struct arg_pair {
            int short_opt;
            std::string_view long_opt;
        };

    public:
        constexpr argument_list(const arg_ele (&in)[N])
            : args{make_args(in)}, slmap{make_map(args)},
              unknown{make_error_code<UnknownCode>(slmap)},
              no_argument{make_error_code<NoArugmentCode>(slmap, unknown)} {}
        constexpr size_t get_max_argc() const { return N; }

        void print_arg() const {
            const int pre_opt           = 2;
            const int short_opt_width   = 2;
            const int long_opt_width    = 25;
            const int arg_width         = pre_opt + short_opt_width + long_opt_width;
            const int explanation_width = 51;

            auto put_chars = [](const int& n, const char c = ' ') -> void {
                for (int i = 0; i < n; ++i)
                    putc(c, stdout);
            };
            for (auto e : args) {
                int out_length = pre_opt;
                int tmp        = 0;
                put_chars(pre_opt);
                switch (e.opt_type()) {
                    case 1:
                        printf("-%c%n", e.short_opt, &tmp);
                        out_length += tmp;
                        put_chars(long_opt_width);
                        out_length += long_opt_width;
                        break;
                    case 2:
                        put_chars(short_opt_width);
                        out_length += short_opt_width;
                        printf("  --%s%n", e.long_opt.data(), &tmp);
                        out_length += tmp;
                        break;
                    case 3:
                        printf("-%c%n", e.short_opt, &tmp);
                        out_length += tmp;
                        printf(", --%s%n", e.long_opt.data(), &tmp);
                        out_length += tmp;
                        break;
                    default:
                        break;
                }

                if (arg_width >= out_length) {
                    put_chars(arg_width - out_length);
                } else {
                    puts("");
                    put_chars(arg_width);
                }

                if (e.explanation.length() <= explanation_width && e.explanation.find('\n') == std::string_view::npos) {
                    printf("%s\n", e.explanation.data());
                } else {
                    std::string_view words = e.explanation;
                    std::string w;
                    out_length = 0;
                    while (true) {
                        auto n = words.find_last_of(' ', explanation_width);
                        if (const auto m = words.find_first_of('\n'); m != std::string_view::npos && n > m) {
                            n = m;
                        }
                        w = words.substr(0, n);
                        printf("%s\n", w.data());
                        // if (n + 1 <= words.length()) {
                        if (n != std::string_view::npos) {
                            words.remove_prefix(n + 1);
                            put_chars(arg_width);
                        } else {
                            puts("");
                            break;
                        }
                    }
                }
            }
        };

        const std::array<arg_ele, N> args;
        const std::array<arg_pair, N> slmap;
        const int unknown;
        const int no_argument;
        static constexpr size_t MAX_ARGC = N;

        constexpr int operator()(const int sopt) const {
            for (const auto& a : args) {
                if (a.short_opt == sopt) {
                    return sopt;
                }
            }
            assert(false);
        }
        constexpr int operator()(const std::string_view& lopt) const {
            for (const auto& sl : slmap) {
                if (sl.long_opt == lopt) {
                    return sl.short_opt;
                }
            }
            assert(false);
        }
        constexpr int operator()(const int sopt, const std::string_view& lopt) const {
            for (const auto& sl : slmap) {
                if ((sopt == '\0' || sl.short_opt == sopt) && sl.long_opt == lopt) {
                    return sl.short_opt;
                }
            }
            assert(false);
        }

        constexpr int long_to_short(const std::string_view& lopt) const {
            int tmp = unknown;
            for (const auto& sl : slmap) {
                if (sl.long_opt.substr(0, lopt.size()) == lopt) {
                    if (sl.long_opt.size() == lopt.size()) {
                        return sl.short_opt;
                    } else {
                        if (tmp == unknown) {
                            tmp = sl.short_opt;
                        } else {
                            return unknown;
                        }
                    }
                }
            }
            return tmp;
        }

        constexpr bool check() const {
            if (unknown == no_argument) {
                return false;
            }
            for (size_t i = 0; i < N - 1; ++i) {
                for (size_t j = i + 1; j < N; ++j) {
                    if (args[i].short_opt == args[j].short_opt && args[i].short_opt != '\0') {
                        return false;
                    }
                    if (args[i].short_opt == unknown || args[i].short_opt == no_argument) {
                        return false;
                    }
                    if (args[i].long_opt == args[j].long_opt && !args[i].long_opt.empty()) {
                        return false;
                    }
                }
            }
            return true;
        }

    private:
        static constexpr std::array<arg_ele, N> make_args(const arg_ele (&in)[N]) {
            std::array<arg_ele, N> output = {};
            int replace                   = 256;
            for (size_t i = 0; i < N; ++i) {
                output[i] = in[i];
                if (output[i].short_opt == 0) {
                    output[i].short_opt = (replace++);
                }
            }
            return output;
        }
        static constexpr std::array<arg_pair, N> make_map(const std::array<arg_ele, N>& in) {
            auto swap = [](arg_pair& l, arg_pair& r) -> void {
                auto tmp = l;
                l        = r;
                r        = tmp;
            };
            std::array<arg_pair, N> output{};
            for (size_t i = 0; i < N; ++i) {
                output[i].short_opt = in[i].short_opt;
                output[i].long_opt  = in[i].long_opt;
            }
            for (size_t i = 0; i < N; ++i) {
                for (size_t j = 0; j + 1 < N - i; ++j) {
                    if (output[j].long_opt > output[j + 1].long_opt) {
                        swap(output[j], output[j + 1]);
                    }
                }
            }
            return output;
        }
        template <int Code>
        static constexpr const int make_error_code(const std::array<arg_pair, N>& in, const int def = 0) {
            if constexpr (Code == UnknownCode) return UnknownCode;
            if constexpr (Code == NoArugmentCode) return NoArugmentCode;
            int output = def - 1;
            while (true) {
                for (const auto& opt : in) {
                    if (output == opt.short_opt) {
                        --output;
                        break;
                    }
                }
                return output;
            }
        }
    };

    template <size_t N, int UnknownCode, int NoArugmentCode>
    class argument_t {
        struct result_pair {
            std::string_view string;
            bool result;
            operator bool() const { return this->result; }
            operator std::string_view() const { return this->string; }
        };

    public:
        argument_t(const int argc, char** argv, const argument_list<N, UnknownCode, NoArugmentCode>& list)
            : list{list},
              load{},
              argv{argv},
              argc{argc},
              current{1},
              is_parsing{false} {}

        std::string_view get_process() const { return *argv; }
        std::string_view show() const { return argv[current - 1]; }
        void restart() { load = {}, current = 1; }
        bool empty() const { return current >= argc && load.empty(); }

        [[nodiscard]] int get_opt() {
            size_t opt_find = 0;
            if (!is_parsing || load.empty()) {
                while (true) {
                    if (!load_argv()) {
                        return 0;
                    }
                    opt_find = load.find_first_not_of('-');
                    if (opt_find) {
                        break;
                    } else {
                        load = {};
                    }
                }

                is_parsing = true;
            }
            load.remove_prefix(opt_find);

            if (opt_find == 2) { // long option
                const auto equal = load.find_first_of('=');
                if (equal == std::string_view::npos) {
                    return list.long_to_short(std::exchange(load, {}));
                } else {
                    int out;
                    if (equal + 1 == load.size()) {
                        out = list.no_argument;
                    } else {
                        out = list.long_to_short(load.substr(0, equal));
                    }
                    load.remove_prefix(equal + 1);
                    return out;
                }
            } else { // short option
                auto out = static_cast<const int>(load.front());
                load.remove_prefix(1);
                return out;
            }
        }

        std::string_view pop() {
            is_parsing = false;
            if (load_argv()) {
                return std::exchange(load, {});
            } else {
                return {};
            }
        }

    private:
        constexpr result_pair load_argv() {
            result_pair output = {load, true};
            if (load.empty()) {
                if (current >= argc) {
                    output.result = false;
                } else {
                    load = argv[current++];
                }
            }
            return output;
        }

        const argument_list<N, UnknownCode, NoArugmentCode>& list;
        std::string_view load;
        const char* const* const argv;
        const int argc;
        int current;
        bool is_parsing;
    };
}