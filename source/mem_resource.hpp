#pragma once
#include <array>
#include <memory_resource>
#include <cstddef>
#include <utility>
#include <type_traits>
#include <cstdlib>

template <typename... Ts>
class j2k_resource : public std::pmr::memory_resource {
private:
    template <typename Head, typename... Tail>
    struct is_nothrow {
        static constexpr bool value = is_nothrow<Tail...>::value;
    };
    template <typename Head>
    struct is_nothrow<Head> {
        static constexpr bool value = std::is_same_v<std::nothrow_t, Head>;
    };
    template <typename Head, typename... Tail>
    static constexpr bool is_nothrow_v = is_nothrow<Head, Tail...>::value;

    template <typename T>
    static constexpr auto siz_v = std::max(sizeof(T), alignof(T));
    static constexpr size_t get_num_unique_align() {
        size_t als[] = {alignof(Ts)...};
        for (size_t i = 0; i < NUM_ARGS; ++i) {
            for (size_t j = 0; j + 1 < NUM_ARGS - i; ++j) {
                if (als[j] < als[j + 1]) {
                    auto tmp   = als[j];
                    als[j]     = als[j + 1];
                    als[j + 1] = tmp;
                }
            }
        }
        size_t out = 1;
        for (size_t i = 0; i < NUM_ARGS - 1; ++i) {
            if (als[i] != als[i + 1]) {
                ++out;
            }
        }
        return out;
    }
    static constexpr auto get_unique_aligns() {
        size_t als[] = {alignof(Ts)...};
        for (size_t i = 0; i < NUM_ARGS; ++i) {
            for (size_t j = 0; j + 1 < NUM_ARGS - i; ++j) {
                if (als[j] < als[j + 1]) {
                    auto tmp   = als[j];
                    als[j]     = als[j + 1];
                    als[j + 1] = tmp;
                }
            }
        }
        std::array<size_t, NUM_UNIQUE_ALIGN> out{als[0]};
        auto it = out.begin() + 1;
        for (size_t i = 0; i < NUM_ARGS - 1; ++i) {
            if (als[i] != als[i + 1]) {
                *it++ = als[i + 1];
            }
        }
        return out;
    }
    static constexpr size_t NUM_ARGS         = sizeof...(Ts);
    static constexpr size_t NUM_UNIQUE_ALIGN = get_num_unique_align();
    static constexpr auto unique_aligns      = get_unique_aligns();

    template <size_t N, typename Head, typename... Tail>
    static constexpr auto tuple_parse_tail(Head h, Tail... ts) {
        if constexpr (sizeof...(Tail) != N) {
            return std::tuple_cat(std::make_tuple(h), tuple_parse_tail<N>(ts...));
        } else {
            return std::make_tuple(h);
        }
    }
    template <typename T, size_t N, typename Head, typename... Tail>
    static constexpr std::array<T, N> tuple_to_array_impl(std::array<T, N> arr, Head h, Tail... ts) {
        arr[N - (sizeof...(Tail) + 1)] = h;
        if constexpr (sizeof...(Tail) != 0) {
            return tuple_to_array_impl(arr, ts...);
        } else {
            return arr;
        }
    }
    template <typename T, typename... Args>
    static constexpr auto tuple_to_array(std::tuple<Args...> tp) {
        return std::apply(tuple_to_array_impl<T, std::tuple_size_v<decltype(tp)>, Args...>, std::tuple_cat(std::make_tuple(std::array<T, std::tuple_size_v<decltype(tp)>>{}), tp));
    }
    template <typename... Args>
    static constexpr auto pack_expansion(Args... args) {
        if constexpr (is_nothrow_v<Args...>) {
            return tuple_to_array<size_t>(tuple_parse_tail<1>(args...));
        } else {
            return std::array<size_t, sizeof...(Args)>{static_cast<size_t>(args)...};
        }
    }

    std::array<std::uintptr_t, NUM_UNIQUE_ALIGN> offsets;
    std::array<std::uintptr_t, NUM_UNIQUE_ALIGN> end_offsets;
    std::byte* memory_pointer;

public:
    j2k_resource()                    = default;
    j2k_resource(const j2k_resource&) = delete;
    j2k_resource(j2k_resource&& other) : j2k_resource{} {
        this->offsets        = other.offsets;
        this->end_offsets    = other.end_offsets;
        this->memory_pointer = std::exchange(other.memory_pointer, nullptr);
    }
    template <typename... Args>
    j2k_resource(Args... args) : j2k_resource{} { prev_allocate(args...); }
    ~j2k_resource() {
        if (memory_pointer != nullptr) {
            free(memory_pointer);
        }
    }
    j2k_resource& operator=(const j2k_resource&) = delete;
    j2k_resource& operator=(j2k_resource&& other) {
        this->offsets        = other.offsets;
        this->end_offsets    = other.end_offsets;
        this->memory_pointer = std::exchange(other.memory_pointer, nullptr);
        return *this;
    }

    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        if (!is_allocated()) throw std::bad_alloc{};
        for (size_t i = 0; i < NUM_UNIQUE_ALIGN; ++i) {
            if (unique_aligns[i] == alignment) {
                if (offsets[i] + bytes > end_offsets[i]) throw std::bad_alloc{};
                return memory_pointer + std::exchange(offsets[i], offsets[i] + bytes);
            }
        }
        throw std::bad_alloc{};
    }
    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment = alignof(std::max_align_t)) override {
        for (size_t i = 0; i < NUM_UNIQUE_ALIGN; ++i) {
            if (unique_aligns[i] == alignment) {
                if (memory_pointer + (offsets[i] - bytes) == p) {
                    offsets[i] -= bytes;
                }
                return;
            }
        }
    }
    bool do_is_equal(const memory_resource& other) const noexcept override { return this == &other; }

    bool is_allocated() const { return memory_pointer != nullptr; }
    template <typename... Args>
    bool prev_allocate(Args... args) noexcept(is_nothrow_v<Args...>) {
        static_assert(
            sizeof...(Args) - static_cast<size_t>(is_nothrow_v<Args...>) == NUM_ARGS &&
            (static_cast<size_t>(std::is_convertible_v<size_t, Args>) + ...) == NUM_ARGS
        );
        constexpr std::array aligns{alignof(Ts)...};
        constexpr std::array sizes{siz_v<Ts>...};

        const auto type_lengths = pack_expansion(args...);
        size_t alloc_size       = 0;

        for (size_t i = 0; i < NUM_UNIQUE_ALIGN; ++i) {
            if (const size_t mod = alloc_size % unique_aligns[i]; mod != 0) {
                alloc_size += unique_aligns[i] - mod;
            }
            offsets[i] = alloc_size;
            for (size_t j = 0; j < NUM_ARGS; ++j) {
                if (aligns[j] == unique_aligns[i]) {
                    alloc_size += type_lengths[j] * sizes[j];
                }
            }
            end_offsets[i] = alloc_size;
        }

        if (memory_pointer = static_cast<std::byte*>(malloc(alloc_size)); memory_pointer == nullptr) {
            if constexpr (is_nothrow_v<Args...>) {
                return false;
            } else {
                throw std::bad_alloc{};
            }
        }
        return true;
    }
};

template <>
class j2k_resource<> : public std::pmr::memory_resource {};

template <typename... Ts>
class j2k_parent_resource;

class j2k_child_resource : public std::pmr::memory_resource {
    template <typename... Ts>
    friend class j2k_parent_resource;

private:
    std::byte* pointer;
    size_t length;

public:
    void* do_allocate(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t)) override {
        if (bytes > length || reinterpret_cast<uintptr_t>(pointer) % alignment) {
            throw std::bad_alloc{};
        }
        length -= bytes;
        return std::exchange(pointer, pointer + bytes);
    }
    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment = alignof(std::max_align_t)) override {
        if (p == pointer - bytes) {
            pointer -= bytes;
            length += bytes;
        }
    }
    bool do_is_equal(const memory_resource& other) const noexcept override { return this == &other; }

    j2k_child_resource() = default;
};

template <size_t Size, size_t Align>
struct virtual_type_t {};

template <typename... Ts>
class j2k_parent_resource {
private:
    static constexpr size_t NUM_TYPES = sizeof...(Ts);
    std::array<j2k_child_resource, NUM_TYPES> pmr_resources;
    std::byte* memory_pointer;

    template <typename T>
    struct size_expansion {
        static constexpr size_t value = sizeof(T);
    };
    template <size_t Size, size_t Align>
    struct size_expansion<virtual_type_t<Size, Align>> {
        static constexpr size_t value = Size;
    };
    template <typename T>
    struct align_expansion {
        static constexpr size_t value = alignof(T);
    };
    template <size_t Size, size_t Align>
    struct align_expansion<virtual_type_t<Size, Align>> {
        static constexpr size_t value = Align;
    };

public:
    j2k_parent_resource()                                 = default;
    j2k_parent_resource(const j2k_parent_resource& other) = delete;
    j2k_parent_resource(j2k_parent_resource&& other) : j2k_parent_resource{} {
        *this = std::move(other);
    }
    template <typename... Args>
    j2k_parent_resource(Args... args) : j2k_parent_resource{} { prev_allocate(args...); }

    ~j2k_parent_resource() { post_deallocate(); }

    j2k_parent_resource& operator=(const j2k_parent_resource& other) = delete;
    j2k_parent_resource& operator=(j2k_parent_resource&& other) {
        this->pmr_resources  = std::exchange(other.pmr_resources, {});
        this->memory_pointer = std::exchange(other.memory_pointer, nullptr);
        return *this;
    }

    bool is_allocated() const { return memory_pointer != nullptr; }

    template <typename... Args>
    size_t prev_allocate(std::nothrow_t, Args... args) noexcept {
        constexpr size_t sizes[NUM_TYPES]  = {size_expansion<Ts>::value...};
        constexpr size_t aligns[NUM_TYPES] = {align_expansion<Ts>::value...};

        const size_t alloc_sizes[NUM_TYPES] = {static_cast<size_t>(args)...};
        size_t total_alloc_size             = 0;

        for (size_t i = 0; i < NUM_TYPES; ++i) {
            if (const auto mod = total_alloc_size % aligns[i]; mod != 0) {
                total_alloc_size += aligns[i] - mod;
            }
            total_alloc_size += alloc_sizes[i] * sizes[i];
            pmr_resources[i].length = alloc_sizes[i] * sizes[i];
        }

        memory_pointer = static_cast<std::byte*>(malloc(total_alloc_size));
        if (memory_pointer == nullptr) {
            return 0;
        }

        pmr_resources.front().pointer = memory_pointer;
        for (size_t i = 1; i < NUM_TYPES; ++i) {
            pmr_resources[i].pointer = memory_pointer + alloc_sizes[i - 1] * sizes[i - 1];
        }
        return total_alloc_size;
    }
    template <typename... Args>
    size_t prev_allocate(Args... args) {
        auto alloc_size = prev_allocate(std::nothrow, args...);
        if (!alloc_size) { throw std::bad_alloc{}; }
        return alloc_size;
    }

    void post_deallocate() {
        if (is_allocated()) {
            free(std::exchange(memory_pointer, nullptr));
            pmr_resources = {};
        }
    }

    template <size_t Index>
    j2k_child_resource* get_resource() { return &pmr_resources.at(Index); }
};
