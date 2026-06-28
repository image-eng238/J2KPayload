#pragma once
#include <cstddef>
#include <cstdint>

template <typename T>
constexpr auto maximum_value_v = static_cast<T>(static_cast<T>(~T{}) > 0 ? ~T{} : ~T{} ^ static_cast<T>(1) << sizeof(T) * 8 - 1);
template <typename T>
constexpr auto minimum_value_v = static_cast<T>(static_cast<T>(~T{}) > 0 ? T{} : static_cast<T>(1) << sizeof(T) * 8 - 1);
static_assert(maximum_value_v<int8_t> == INT8_MAX && minimum_value_v<int8_t> == INT8_MIN);
static_assert(maximum_value_v<uint8_t> == UINT8_MAX && minimum_value_v<uint8_t> == 0);
static_assert(maximum_value_v<int16_t> == INT16_MAX && minimum_value_v<int16_t> == INT16_MIN);
static_assert(maximum_value_v<uint16_t> == UINT16_MAX && minimum_value_v<uint16_t> == 0);
static_assert(maximum_value_v<int32_t> == INT32_MAX && minimum_value_v<int32_t> == INT32_MIN);
static_assert(maximum_value_v<uint32_t> == UINT32_MAX && minimum_value_v<uint32_t> == 0);
static_assert(maximum_value_v<int64_t> == INT64_MAX && minimum_value_v<int64_t> == INT64_MIN);
static_assert(maximum_value_v<uint64_t> == UINT64_MAX && minimum_value_v<uint64_t> == 0);

template <size_t N>
constexpr bool is_pow2_v = [](size_t n = N) { for (; !(n & 1); n >>= 1); return !(n & SIZE_MAX - 1); }();
template <>
constexpr bool is_pow2_v<0> = true;

template <typename T, T Max, T Min>
class range_wrap_t {
public:
    using value_type                = T;
    static constexpr value_type max = Max;
    static_assert(maximum_value_v<value_type> > Max);
    static constexpr value_type min = Min;
    static_assert(minimum_value_v<value_type> <= Min);
    static_assert(is_pow2_v<Max + 1>);

    constexpr range_wrap_t() : value{} {};
    constexpr range_wrap_t(value_type n) : value{n} {}

    constexpr range_wrap_t& operator=(value_type n) {
        value = n;
        return *this;
    }
    constexpr range_wrap_t& operator++() {
        ++value;
        return *this;
    }
    constexpr range_wrap_t operator++(int) {
        const auto tmp = *this;
        value++;
        return tmp;
    }
    constexpr range_wrap_t& operator--() {
        --value;
        return *this;
    }
    constexpr range_wrap_t operator--(int) {
        const auto tmp = *this;
        value--;
        return tmp;
    }
    constexpr operator value_type() {
        return get();
    }

    constexpr bool is_overflow() const { return value > Max; }
    constexpr value_type get() const { return value & Max; }
    constexpr value_type get_raw() const { return value; }

    constexpr void set(value_type n) { value = n; }
    constexpr void wraparound() { value = get(); }

protected:
    value_type value;
};
