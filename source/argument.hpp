#pragma once
#include <array>
#include <cassert>
#include <charconv>
#include <cstddef>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <cstdio>
#include <string>

#define TKLIB_ARG_DESCRIPTION(opts, fmt) (fmt).char_array<(fmt).length((opts))>((opts)).data()

namespace tklib {
    template <typename T, typename Head, typename... Tail>
    struct count_type {
        static constexpr size_t value = static_cast<size_t>(std::is_same_v<T, Head>) + count_type<T, Tail...>::value;
    };
    template <typename T, typename Head>
    struct count_type<T, Head> {
        static constexpr size_t value = static_cast<size_t>(std::is_same_v<T, Head>);
    };
    template <typename T, typename Head, typename... Tail>
    constexpr size_t count_type_v = count_type<T, Head, Tail...>::value;

    struct opt_t {
        int short_opt;
        bool has_arg;
        std::string_view long_opt;
        constexpr opt_t() : short_opt{}, has_arg{}, long_opt{} {}
        constexpr opt_t(int sopt) : short_opt{sopt}, has_arg{}, long_opt{} {}
        constexpr void swap(opt_t& in) {
            const opt_t tmp = *this;
            *this           = in;
            in              = tmp;
        }
    };
    bool operator==(const opt_t& lsh, const opt_t& rsh) { return lsh.short_opt == rsh.short_opt && lsh.long_opt == rsh.long_opt; }
    bool operator!=(const opt_t& lsh, const opt_t& rsh) { return !(lsh == rsh); }

    struct optspec_t {
        constexpr optspec_t() : short_opt{}, long_opt{}, has_arg{}, explanation{} {}
        constexpr optspec_t(int short_opt, std::string_view long_opt, bool has_arg, std::string_view explanation) : short_opt{short_opt}, long_opt{long_opt}, has_arg{has_arg}, explanation{explanation} {}
        int short_opt;
        std::string_view long_opt;
        bool has_arg;
        std::string_view explanation;
        constexpr auto make_opt_t() const {
            opt_t tmp{};
            tmp.has_arg   = this->has_arg;
            tmp.short_opt = this->short_opt;
            tmp.long_opt  = this->long_opt;
            return tmp;
        }
        enum {
            short_only = 0b01,
            long_only  = 0b10,
            both       = 0b11
        };
        constexpr int opt_type() const { return !!isalnum(short_opt) | !long_opt.empty() << 1; }
    };

    struct optspec_old_t : public optspec_t {
        constexpr optspec_old_t(int short_opt, std::string_view long_opt, std::string_view explanation) : optspec_t{short_opt, long_opt, true, explanation} {};
    };

    enum class opt_err : size_t {
        unknown,
        ambiguous,
        no_argument,
        positional_argument,
    };

    template <typename T>
    struct interval_t {
        T first;
        T last;
    };

    template <size_t N>
    class argument_list {
    private:
        using args_t  = std::array<optspec_t, N>;
        using table_t = std::array<opt_t, N>;
        using ecode_t = std::array<int, 4>;

        const args_t args;
        const table_t s_sort_table;
        const table_t l_sort_table;
        const size_t num_long_opt;
        const ecode_t error_code;
        static constexpr size_t NUM_OPTION = N;

    public:
        constexpr argument_list(const optspec_t (&in)[N])
            : args{init_args(from_arr(in))},
              s_sort_table{init_s_sort_table(args)},
              l_sort_table{init_l_sort_table(args)},
              num_long_opt{init_num_long_opt(l_sort_table)},
              error_code{init_error_code(l_sort_table)} { assert(check()); }

        template <typename... Args>
        constexpr argument_list(Args... in)
            : args{init_args(from_va(in...))},
              s_sort_table{init_s_sort_table(args)},
              l_sort_table{init_l_sort_table(args)},
              num_long_opt{init_num_long_opt(l_sort_table)},
              error_code{init_error_code(l_sort_table)} { assert(check()); }

        [[deprecated("please specify true or false there are arguments option")]] constexpr argument_list(const optspec_old_t (&in)[N])
            : args{init_args(from_arr_old(in))},
              s_sort_table{init_s_sort_table(args)},
              l_sort_table{init_l_sort_table(args)},
              num_long_opt{init_num_long_opt(l_sort_table)},
              error_code{init_error_code(l_sort_table)} { assert(check()); }

        constexpr const args_t& access_args() const { return args; }

        constexpr int
        operator()(const int sopt) const {
            for (const auto& a : args) {
                if (a.short_opt == sopt) {
                    return sopt;
                }
            }
            assert(false);
        }
        constexpr int operator()(const std::string_view& lopt) const {
            for (const auto& sl : l_sort_table) {
                if (sl.long_opt == lopt) {
                    return sl.short_opt;
                }
            }
            assert(false);
        }
        constexpr int operator()(const int sopt, const std::string_view& lopt) const {
            for (const auto& ls : l_sort_table) {
                if ((sopt == '\0' || ls.short_opt == sopt) && ls.long_opt == lopt) {
                    return ls.short_opt;
                }
            }
            assert(false);
        }
        constexpr int operator()(const opt_t& opt) const {
            for (const auto& ls : l_sort_table) {
                if (ls == opt) return ls.short_opt;
            }
            assert(false);
        }
        constexpr int operator()(opt_err n) const {
            return error_code.at(static_cast<size_t>(n));
        }

        constexpr bool is_valid(int val) const {
            for (const auto& e : error_code) {
                if (e == val) return false;
            }
            return true;
        }
        constexpr int long_to_short(const std::string_view& lopt) const {
            const size_t start = [&] {
                size_t out = 0;
                for (; out < num_long_opt && l_sort_table[out].long_opt.front() != lopt.front(); ++out);
                return out;
            }();
            const size_t stop = [&] {
                size_t out = start;
                for (; out < num_long_opt && l_sort_table[out].long_opt.front() == lopt.front(); ++out);
                return out;
            }();

            int tmp = (*this)(opt_err::unknown);
            for (size_t i = start; i < stop; ++i) {
                auto& ls = l_sort_table[i];
                auto str = ls.long_opt.substr(0, lopt.size());
                if (str == lopt) {
                    if (ls.long_opt.size() == lopt.size()) {
                        return ls.short_opt;
                    } else {
                        if (tmp == (*this)(opt_err::unknown)) {
                            tmp = ls.short_opt;
                        } else {
                            return (*this)(opt_err::ambiguous);
                        }
                    }
                }
            }
            return tmp;
        }
        constexpr opt_t get_option(int sopt, bool is_valid = false) const {
            auto tmp   = s_sort_table[s_sort_table.size() / 2];
            auto start = 0;
            auto stop  = s_sort_table.size();
            if (tmp.short_opt > sopt) {
                stop = s_sort_table.size() / 2;
            } else {
                start = s_sort_table.size() / 2;
            }
            for (size_t i = start; i < stop; ++i) {
                if (s_sort_table[i].short_opt == sopt) { return s_sort_table[i]; }
            }
            if (is_valid) {
                assert(false);
            } else {
                return opt_t{(*this)(opt_err::unknown)};
            }
        }
        constexpr opt_t get_option(const std::string_view& lopt, bool is_valid = false) const {
            const size_t start = [&] {
                size_t out = 0;
                for (; out < num_long_opt && l_sort_table[out].long_opt.front() != lopt.front(); ++out);
                return out;
            }();
            const size_t stop = [&] {
                size_t out = start;
                for (; out < num_long_opt && l_sort_table[out].long_opt.front() == lopt.front(); ++out);
                return out;
            }();

            opt_t tmp{(*this)(opt_err::unknown)};
            for (size_t i = start; i < stop; ++i) {
                auto& ls = l_sort_table[i];
                auto str = ls.long_opt.substr(0, lopt.size());
                if (str == lopt) {
                    if (ls.long_opt.size() == lopt.size()) {
                        return ls;
                    } else {
                        if (tmp == (*this)(opt_err::unknown)) {
                            tmp = ls;
                        } else {
                            assert(!is_valid);
                            return opt_t{(*this)(opt_err::ambiguous)};
                        }
                    }
                }
            }
            if (is_valid) assert(this->is_valid(tmp.short_opt));
            return tmp;
        }
        constexpr size_t count_ambiguous(const std::string_view& lopt) const {
            assert(!lopt.empty());
            const size_t start = [&] {
                size_t out = 0;
                for (; out < num_long_opt && l_sort_table[out].long_opt.front() != lopt.front(); ++out);
                return out;
            }();
            const size_t finish = [&] {
                size_t out = start;
                for (; out < num_long_opt && l_sort_table[out].long_opt.front() == lopt.front(); ++out);
                return out;
            }();
            size_t out = 0;
            for (size_t i = start; i < finish; ++i) {
                auto& ls = l_sort_table[i];
                auto str = ls.long_opt.substr(0, lopt.size());
                if (str == lopt) {
                    ++out;
                }
            }
            return out;
        }
        template <typename Iterator>
        constexpr Iterator get_ambiguous(const std::string_view& lopt, Iterator begin, Iterator end) const {
            assert(!lopt.empty());
            const size_t start = [&] {
                size_t out = 0;
                for (; out < num_long_opt && l_sort_table[out].long_opt.front() != lopt.front(); ++out);
                return out;
            }();
            const size_t finish = [&] {
                size_t out = start;
                for (; out < num_long_opt && l_sort_table[out].long_opt.front() == lopt.front(); ++out);
                return out;
            }();
            auto it = begin;
            for (size_t i = start; i < std::min(finish, static_cast<size_t>(std::distance(begin, end) + 1)); ++i) {
                auto& ls = l_sort_table[i];
                if (ls.long_opt.substr(0, lopt.size()) == lopt) {
                    *it++ = ls.long_opt;
                }
            }
            return it;
        }

        [[deprecated("please use 'arg_description_fmt::length' & 'arg_description_fmt::char_array' function")]]
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
                    case optspec_t::short_only:
                        printf("-%c%n", e.short_opt, &tmp);
                        out_length += tmp;
                        put_chars(long_opt_width);
                        out_length += long_opt_width;
                        break;
                    case optspec_t::long_only:
                        put_chars(short_opt_width);
                        out_length += short_opt_width;
                        printf("  --%s%n", e.long_opt.data(), &tmp);
                        out_length += tmp;
                        break;
                    case optspec_t::both:
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
                        auto n = words.find_first_of(' ', explanation_width);
                        if (const auto m = words.find_first_of('\n'); m != std::string_view::npos && n > m) {
                            n = m;
                        }
                        if (n < explanation_width) {
                            w = words;
                            n = std::string_view::npos;
                        } else {
                            w = words.substr(0, n);
                        }
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

        constexpr bool check() const {
            for (size_t i = 0; i < N - 1; ++i) {
                for (size_t j = i + 1; j < N; ++j) {
                    if (args[i].short_opt == args[j].short_opt && args[i].short_opt != '\0') {
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
        static constexpr args_t from_arr(const optspec_t (&in)[N]) {
            args_t out{};
            for (size_t i = 0; i < N; ++i) out[i] = in[i];
            return out;
        }
        static constexpr args_t from_arr_old(const optspec_old_t (&in)[N]) {
            args_t out{};
            for (size_t i = 0; i < N; ++i) out[i] = in[i];
            return out;
        }
        template <typename... Args>
        static constexpr args_t from_va(Args... in) {
            std::array<optspec_t, sizeof...(Args)> pack_expansion{static_cast<optspec_t>(in)...};
            args_t out{};
            for (size_t i = 0; i < out.size(); ++i) { out[i] = pack_expansion[i]; }
            return out;
        }
        static constexpr args_t init_args(args_t in) {
            int replace = 256;
            for (size_t i = 0; i < N; ++i) {
                if (in[i].short_opt == 0) {
                    assert(!in[i].long_opt.empty());
                    in[i].short_opt = (replace++);
                }
            }
            return in;
        }
        static constexpr table_t init_s_sort_table(const args_t& in) {
            table_t output{};
            for (size_t i = 0; i < N; ++i) output[i] = in[i].make_opt_t();

            for (size_t i = 0; i < N; ++i) {
                for (size_t j = 0; j + 1 < N - i; ++j) {
                    if (output[j].short_opt > output[j + 1].short_opt) {
                        output[j].swap(output[j + 1]);
                    }
                }
            }
            return output;
        }
        static constexpr table_t init_l_sort_table(const args_t& in) {
            table_t output{};
            for (size_t i = 0; i < N; ++i) output[i] = in[i].make_opt_t();

            for (size_t i = 0; i < N; ++i) {
                for (size_t j = 0; j + 1 < N - i; ++j) {
                    if (output[j].long_opt.empty() || (output[j].long_opt > output[j + 1].long_opt && !output[j + 1].long_opt.empty())) {
                        output[j].swap(output[j + 1]);
                    }
                }
            }
            return output;
        }
        static constexpr size_t init_num_long_opt(const table_t& in) {
            size_t out = 0;
            for (; (out < in.size()) && (!in[out].long_opt.empty()); ++out);
            return out;
        }
        static constexpr ecode_t init_error_code(const table_t& in) {
            ecode_t out{};
            auto is_unique_in = [&](int val) -> bool {
                for (const auto& e : in) {
                    if (e.short_opt == val) return false;
                }
                return true;
            };
            auto is_unique_out = [&](int val) -> bool {
                for (const auto& e : out) {
                    if (e == val) return false;
                }
                return true;
            };
            int code = -1;
            for (auto& e : out) {
                while (!is_unique_in(code) || !is_unique_out(code)) {
                    --code;
                }
                e = code;
            }
            return out;
        }
    };
    template <typename... Args>
    argument_list(Args...) -> argument_list<count_type_v<optspec_t, Args...>>;

    struct arg_description_fmt {
        int opt_offset    = 2;
        int desc_offset_x = 29;
        int desc_offset_y = 0;
        int max_width     = 82;

        template <size_t N>
        constexpr size_t length(const argument_list<N>& opts) const {
            assert(max_width > desc_offset_x);
            assert(desc_offset_y >= 0);
            auto args  = opts.access_args();
            size_t out = 0;

            const auto sopt_width = 4;
            const auto lopt_ps    = 3; // Prefix "--" + suffix '  'or'\n'
            for (const optspec_t& arg : args) {
                auto optlen = opt_offset + sopt_width;
                optlen += !arg.long_opt.empty() ? arg.long_opt.length() + lopt_ps : 0;
                assert(optlen <= max_width);
                if (desc_offset_y == 0) {
                    if (optlen <= desc_offset_x) {
                        out += desc_offset_x;
                    } else {
                        out += optlen + desc_offset_x;
                    }
                } else {
                    out += optlen + desc_offset_x;
                    out += desc_offset_y - 1;
                }

                std::string_view desc   = arg.explanation;
                const size_t desc_width = max_width - desc_offset_x;
                int space = -1, linebk = -1;
                for (size_t i = 0; i < desc.length(); ++i) {
                    size_t j = 0;
                    for (; j < desc_width && i + j < desc.length(); ++j) {
                        if (desc[i + j] == ' ') {
                            space = j;
                        }
                        if (desc[i + j] == '\n') {
                            linebk = i + j;
                            break;
                        }
                    }
                    if (linebk != -1) {
                        out += j + desc_offset_x + 1;
                        i += j;
                        linebk = -1;
                    } else if (space != -1 && i + j != desc.length()) {
                        out += space + desc_offset_x + 1;
                        i += space;
                        space = -1;
                    } else if (i + j == desc.length()) {
                        out += j + 1; // j + '\n'
                        break;
                    } else {
                        out += j + desc_offset_x + 1;
                        i += j - 1;
                    }
                }
            };
            return out;
        }
        template <size_t N, size_t M>
        constexpr std::array<char, N> char_array(const argument_list<M>& opts) const {
            assert(max_width > desc_offset_x);
            assert(desc_offset_y >= 0);
            const auto sopt_width = 4;
            const auto lopt_ps    = 3; // Prefix "--" + suffix '  'or'\n'
            auto args             = opts.access_args();
            std::array<char, N> out{};
            std::string_view debug_str{out.data()};
            size_t write_i   = 0;
            size_t write_end = 0;

            auto write_char   = [&](char c) { out[write_i++] = c; };
            auto write_n_char = [&](size_t n, char c = ' ') {
                const auto tmp = write_i + n;
                for (; write_i < tmp; ++write_i) {
                    out[write_i] = c;
                }
            };
            auto write_str = [&](std::string_view str) {
                for (const auto& c : str) {
                    write_char(c);
                }
            };
            auto fill_end = [&](char lastc = ' ') {
                for (; write_i < write_end - 1;) {
                    write_char(' ');
                }
                if (write_i < write_end) write_char(lastc);
            };
            auto write_opt = [&](const optspec_t& arg) {
                write_n_char(opt_offset);
                switch (arg.opt_type()) {
                    case optspec_t::short_only:
                        write_char('-');
                        write_char(arg.short_opt);
                        break;
                    case optspec_t::long_only:
                        write_n_char(sopt_width);
                        write_n_char(2, '-');
                        write_str(arg.long_opt);
                        break;
                    case optspec_t::both:
                        write_char('-');
                        write_char(arg.short_opt);
                        write_char(',');
                        write_char(' ');
                        write_n_char(2, '-');
                        write_str(arg.long_opt);
                        break;
                }
            };
            auto write_desc = [&](const std::string_view& str) {
                write_str(str);
                write_char('\n');
                fill_end();
                debug_str = {out.data(), write_end};
                assert(write_i == write_end);
            };

            for (const optspec_t& arg : args) {
                auto optlen = opt_offset + sopt_width;
                optlen += !arg.long_opt.empty() ? arg.long_opt.length() + lopt_ps : 0;
                assert(optlen <= max_width);
                if (desc_offset_y == 0) {
                    if (optlen <= desc_offset_x) {
                        write_end += desc_offset_x;
                        write_opt(arg);
                        fill_end();
                    } else {
                        write_end += optlen + desc_offset_x;
                        write_opt(arg);
                        write_char('\n');
                        fill_end();
                    }
                } else {
                    write_end += optlen + desc_offset_x;
                    write_opt(arg);
                    write_end += desc_offset_y - 1;
                    write_n_char(desc_offset_y - 1, '\n');
                    fill_end();
                }
                debug_str = {out.data(), write_end};
                assert(write_i == write_end);

                std::string_view desc   = arg.explanation;
                const size_t desc_width = max_width - desc_offset_x;
                int space = -1, linebk = -1;
                for (size_t i = 0; i < desc.length(); ++i) {
                    size_t j = 0;
                    for (; j < desc_width && i + j < desc.length(); ++j) {
                        if (desc[i + j] == ' ') {
                            space = j;
                        }
                        if (desc[i + j] == '\n') {
                            linebk = i + j;
                            break;
                        }
                    }

                    if (linebk != -1) {
                        write_end += j + desc_offset_x + 1;
                        write_desc(desc.substr(i, j));
                        i += j;
                        linebk = -1;
                    } else if (space != -1 && i + j != desc.length()) {
                        write_end += space + desc_offset_x + 1;
                        write_desc(desc.substr(i, space));
                        i += space;
                        space = -1;
                    } else if (i + j == desc.length()) {
                        write_end += j + 1; // j + '\n'
                        write_desc(desc.substr(i));
                        break;
                    } else {
                        write_end += j + desc_offset_x + 1;
                        write_desc(desc.substr(i, j));
                        i += j - 1;
                    }
                }
            }
            debug_str = {out.data(), write_end};
            assert(out.back() == '\n');
            out.back() = '\0';
            debug_str  = {out.data(), write_end};
            assert(write_i == write_end && write_i == N);
            return out;
        }
    };
    constexpr const arg_description_fmt dft_style_fmt{};
    constexpr const arg_description_fmt man_style_fmt{4, 8, 0, 79};

    template <size_t N>
    class argument_t {
        const argument_list<N>& opts;
        const char* const* const argv;
        std::string_view parsing;
        std::string_view last_parse;
        std::string_view opt_arg;
        int parsing_option;
        const int argc;
        int current_str;
        const bool use_posisional_argument;
        bool is_positional;

    public:
        argument_t(const int argc, char** argv, const argument_list<N>& opts, const bool use_posisional_argument = false)
            : opts{opts}, parsing{}, last_parse{}, opt_arg{}, parsing_option{},
              argv{argv}, argc{argc}, current_str{},
              use_posisional_argument{use_posisional_argument}, is_positional{false} {}

        std::string_view get_process() const { return *argv; }
        bool can_parse() const { return current_str + 1 < argc && parsing.empty(); }

        [[nodiscard]] int getopt() {
            assert(can_parse());
            if (is_positional && use_posisional_argument) {
                opt_arg = argv[++current_str];
                return opts(opt_err::positional_argument);
            }
            if (parsing.empty()) {
                parsing               = argv[++current_str];
                const size_t sopt_pfx = 1;
                const size_t lopt_pfx = 2;
                const size_t num_pfx  = parsing.find_first_not_of('-');
                if (num_pfx == sopt_pfx) {
                    parsing.remove_prefix(1);
                } else if (num_pfx == lopt_pfx) {
                    const auto equal = parsing.find_first_of('=');
                    if (equal != std::string_view::npos) {
                        parsing.remove_prefix(equal + 1);
                    } else {
                        parsing = {};
                    }
                    last_parse     = parsing_origin().substr(lopt_pfx, equal - lopt_pfx);
                    parsing_option = opts.long_to_short(last_parse);

                    return verify_opt_arg(true);
                } else if (num_pfx == std::string_view::npos && parsing.size() == 2 && use_posisional_argument) {
                    parsing = {};
                    if (current_str + 1 < argc) {
                        opt_arg = argv[++current_str];
                    } else {
                        opt_arg = {};
                    }
                    is_positional = true;
                    return opts(opt_err::positional_argument);
                } else if (use_posisional_argument) {
                    opt_arg = std::exchange(parsing, {});
                    return opts(opt_err::positional_argument);
                } else {
                    return opts(opt_err::unknown);
                }
            }

            last_parse     = parsing.substr(0, 1);
            parsing_option = parsing.front();
            parsing.remove_prefix(1);

            return verify_opt_arg();
        }

        std::string_view get_last_parse() const { return last_parse; }
        std::string_view get_optarg() const { return opt_arg; }
        std::string_view get_str() const {
            assert(get_optarg().data() != nullptr);
            return get_optarg();
        }
        std::optional<std::string_view> get_posarg() const {
            if (auto tmp = get_optarg(); tmp.data()) {
                return tmp;
            } else {
                return std::nullopt;
            }
        }

        template <typename T>
        std::optional<T> get_value() const {
            auto str = get_optarg();
            T out;
            auto r = std::from_chars(str.begin(), str.end(), out);
            if (r.ptr != str.end() || r.ec != std::errc{}) { return std::nullopt; }
            return out;
        }

        template <typename T, typename Iterator>
        Iterator get_values(Iterator begin, Iterator end, char delim = ',') const {
            static_assert(std::is_same_v<T, typename std::iterator_traits<Iterator>::value_type>);
            auto str = get_optarg();
            auto it  = begin;
            for (; it != end; ++it) {
                auto r = std::from_chars(str.begin(), str.end(), *it);
                if (r.ec != std::errc{}) { return it; }
                if (*(r.ptr) != delim) { return ++it; }
                str.remove_prefix(std::distance(str.begin(), r.ptr) + 1);
            }
            return it;
        }
        template <typename T, template <typename> typename Container = std::vector>
        auto get_values(char delim = ',') const {
            const size_t num_delim = [&] {
                auto str   = get_optarg();
                size_t out = 1;
                for (const auto& c : str)
                    if (c == delim) ++out;
                return out;
            }();
            Container<T> container;
            container.resize(num_delim);
            auto result = get_values<T>(container.begin(), container.end(), delim);
            container.erase(result, container.end());
            return std::move(container);
        }

        template <typename T>
        std::optional<interval_t<T>> get_interval(char iv_delim = '-') const {
            return get_interval_impl<T>(get_optarg(), iv_delim);
        }

        template <typename T, typename Iterator>
        Iterator get_intervals(Iterator begin, Iterator end, char delim = ',', char iv_delim = '-') const {
            static_assert(std::is_same_v<interval_t<T>, typename std::iterator_traits<Iterator>::value_type>);
            auto str = get_optarg();
            auto it  = begin;
            for (; it != end && !str.empty(); ++it) {
                const auto pos = std::min(str.find_first_of(delim), str.length());
                auto one       = get_interval_impl<T>({str.begin(), pos}, iv_delim);
                str.remove_prefix(std::min(pos + 1, str.length()));
                if (one.has_value()) {
                    *it = one.value();
                } else {
                    return it;
                }
            }
            return it;
        }
        template <typename T, template <typename> typename Container = std::vector>
        auto get_intervals(char delim = ',', char iv_delim = '-') const {
            const size_t num_delim = [&] {
                auto str   = get_optarg();
                size_t out = 1;
                for (const auto& c : str)
                    if (c == delim) ++out;
                return out;
            }();
            Container<interval_t<T>> container;
            container.resize(num_delim);
            auto result = get_intervals<T>(container.begin(), container.end(), delim, iv_delim);
            container.erase(result, container.end());
            return std::move(container);
        }

        [[deprecated("please use 'can_parse' function")]] bool empty() const { return !can_parse(); }
        [[deprecated("please use 'getopt' function"), nodiscard]] int get_opt() {
            [[maybe_unused]] auto tmp = getopt();
            return parsing_option;
        }
        [[deprecated("please use 'get_optarg' function")]] std::string_view pop() const { return get_optarg(); }
        [[deprecated("please use 'get_last_parse' function")]] std::string_view show() const { return parsing_origin(); }

    private:
        std::string_view parsing_origin() const { return argv[current_str]; }

        int verify_opt_arg(bool parsing_valid = false) {
            if (opts.is_valid(parsing_option) && opts.get_option(parsing_option, parsing_valid).has_arg) {
                if (parsing.empty()) {
                    if (current_str + 1 >= argc) { return opts(opt_err::no_argument); }
                    parsing = argv[++current_str];
                    opt_arg = std::exchange(parsing, {});

                    const auto num_pfx = opt_arg.find_first_not_of('-');
                    if (opt_arg.size() <= 1) return parsing_option;
                    switch (num_pfx) {
                        case 1: {
                            auto opt = opts.get_option(opt_arg[num_pfx]);
                            if (opt.short_opt == opts(opt_err::unknown)) { return parsing_option; }
                            if (!opt.has_arg && opt_arg.size() > 2) {
                                return parsing_option;
                            } else {
                                return opts(opt_err::no_argument);
                            }
                        } break;
                        case 2: {
                            auto opt = opts.get_option(opt_arg.substr(2, opt_arg.find_first_of('=') - 2));
                            if (opt.short_opt == opts(opt_err::unknown)) { return parsing_option; }
                            if (!opt.has_arg && opt_arg.size() > 2) {
                                return parsing_option;
                            } else {
                                return opts(opt_err::no_argument);
                            }
                        } break;
                        default:
                            if (opt_arg == "--") {
                                return opts(opt_err::no_argument);
                            }
                            break;
                    }
                } else {
                    opt_arg = std::exchange(parsing, {});
                }
            } else {
                opt_arg = {};
            }
            return parsing_option;
        }

        template <typename T>
        static std::optional<interval_t<T>> get_interval_impl(std::string_view str, char iv_delim) {
            interval_t<T> out;
            std::from_chars_result r;
            r = std::from_chars(str.begin(), str.end(), out.first);

            if (r.ptr == str.end() && r.ec == std::errc{}) { return interval_t<T>{out.first, out.first}; }
            if (r.ec != std::errc{} || *(r.ptr) != iv_delim) { return std::nullopt; }

            r = std::from_chars(r.ptr + 1, str.end(), out.last);
            if (r.ptr != str.end() || r.ec != std::errc{}) { return std::nullopt; }
            return out;
        }
    };
}