#pragma once
#include <cstdint>
#include <cstddef>
#include <iterator>
#include <exception>

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
        if (n >= size()) {
            throw std::out_of_range{"pointer_with_lengh::at: n"};
        }
        return data()[n];
    }
    constexpr const_reference at(size_type n) const {
        if (n >= size()) {
            throw std::out_of_range{"pointer_with_lengh::at: n"};
        }
        return data()[n];
    }

    constexpr reference operator[](size_type n) noexcept { return data()[n]; }
    constexpr const_reference operator[](size_type n) const noexcept { return data()[n]; }

    constexpr reference front() noexcept { return data()[static_cast<size_type>(0)]; }
    constexpr const_reference front() const noexcept { return data()[static_cast<size_type>(0)]; }

    constexpr reference back() noexcept { return data()[size() - 1]; }
    constexpr const_reference back() const noexcept { return data()[size() - 1]; }

    constexpr pointer data() noexcept { return static_cast<pointer>(ptr); }
    constexpr const_pointer data() const noexcept { return static_cast<const_pointer>(ptr); }

    constexpr iterator begin() noexcept { return iterator(data()); }
    constexpr const_iterator begin() const noexcept { return const_iterator(data()); }

    constexpr iterator end() noexcept { return iterator(data() + size()); }
    constexpr const_iterator end() const noexcept { return const_iterator(data() + size()); }

    constexpr reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    constexpr const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }

    constexpr reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    constexpr const_reverse_iterator rend() const noexcept { const_reverse_iterator(begin()); }

    constexpr const_iterator cbegin() const noexcept { return const_iterator(data()); }

    constexpr const_iterator cend() const noexcept { return const_iterator(data() + size()); }

    constexpr const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(end()); }

    constexpr const_reverse_iterator crend() const noexcept { return const_reverse_iterator(begin()); }

    constexpr size_type size() const noexcept { return len; }
    constexpr size_type max_size() const noexcept { return len; }
    constexpr bool empty() const noexcept { return size() == 0; }
};
using packet_t = pointer_with_length<uint8_t>;