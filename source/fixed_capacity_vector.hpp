#pragma once
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <new>
#include <memory>
#include <utility>

template <typename T, std::size_t N>
class fixed_capacity_vector {
public:
    using value_type             = T;
    using pointer                = value_type*;
    using const_pointer          = const value_type*;
    using reference              = value_type&;
    using const_reference        = const value_type&;
    using iterator               = value_type*;
    using const_iterator         = const value_type*;
    using size_type              = std::size_t;
    using difference_type        = std::ptrdiff_t;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

private:
    static constexpr size_type value_size   = std::max(sizeof(value_type), alignof(value_type));
    static constexpr size_type storage_size = value_size * N;
    size_type len;
    alignas(alignof(T)) std::byte storage[storage_size];

public:
    fixed_capacity_vector() noexcept : len{} {}
    fixed_capacity_vector(size_type n) : fixed_capacity_vector{} { resize(n); }
    fixed_capacity_vector(size_type n, const_reference c) : fixed_capacity_vector{} { resize(n, c); }
    template <typename InputIterator>
    fixed_capacity_vector(InputIterator first, InputIterator last) : fixed_capacity_vector{} {
        if (std::distance(first, last) > max_size()) { std::bad_alloc{}; }
        iterator_assign(first, last);
    }
    fixed_capacity_vector(const fixed_capacity_vector& other) : fixed_capacity_vector{other.begin(), other.end()} {}
    fixed_capacity_vector(fixed_capacity_vector&& other)
        : fixed_capacity_vector{std::move_iterator{other.begin()}, std::move_iterator{other.end()}} {
        other.clear();
    }
    fixed_capacity_vector(std::initializer_list<value_type> il) : fixed_capacity_vector{il.begin(), il.end()} {}

    ~fixed_capacity_vector() noexcept { clear(); }

    fixed_capacity_vector& operator=(const fixed_capacity_vector& other) {
        clear();
        iterator_assign(other.begin(), other.end());
    }
    fixed_capacity_vector& operator=(fixed_capacity_vector&& other) {
        clear();
        iterator_assign(std::move_iterator{other.begin()}, std::move_iterator{other.end()});
        other.clear();
    }
    fixed_capacity_vector& operator=(std::initializer_list<value_type>& il) {
        if (il.size() > max_size()) { std::bad_alloc{}; }
        clear();
        iterator_assign(il.begin(), il.end());
    }

    reference at(size_type n) {
        if (n >= size()) {
            throw std::out_of_range{"fixed_capacity_vector::at: n"};
        }
        return data()[n];
    }
    const_reference at(size_type n) const {
        if (n >= size()) {
            throw std::out_of_range{"fixed_capacity_vector::at: n"};
        }
        return data()[n];
    }

    constexpr reference operator[](size_type n) noexcept { return data()[n]; }
    constexpr const_reference operator[](size_type n) const noexcept { return data()[n]; }

    constexpr reference front() noexcept { return data()[static_cast<size_type>(0)]; }
    constexpr const_reference front() const noexcept { return data()[static_cast<size_type>(0)]; }

    constexpr reference back() noexcept { return data()[size() - 1]; }
    constexpr const_reference back() const noexcept { return data()[size() - 1]; }

    constexpr pointer data() noexcept { return reinterpret_cast<pointer>(storage); }
    constexpr const_pointer data() const noexcept { return reinterpret_cast<const_pointer>(storage); }

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
    static constexpr size_type max_size() noexcept { return N; }
    static constexpr size_type capacity() noexcept { return N; }
    void resize(size_type sz) {
        if (sz > max_size()) { throw std::bad_alloc{}; }
        if (size() < sz) {
            for (; len < sz; ++len) {
                assign_value(data() + len);
            }
        } else if (size() == sz) {
            return;
        } else {
            while (size() > sz) {
                std::destroy_at(data() + (--len));
            }
        }
    }
    void resize(size_type sz, const_reference c) {
        if (sz > max_size()) { throw std::bad_alloc{}; }
        if (size() < sz) {
            for (; len < sz; ++len) {
                assign_value(data() + len, c);
            }
        } else if (size() == sz) {
            return;
        } else {
            while (size() >= sz) {
                std::destroy_at(data() + (--len));
            }
        }
    }
    constexpr bool empty() const noexcept { return size() == 0; }
    static void reserve(size_type n) {
        if (n < capacity()) { std::bad_alloc{}; }
    }
    static void shrink_to_fit() noexcept {}

    template <typename InputIterator>
    void assign(InputIterator first, InputIterator last) {
        if (std::distance(first, last) > max_size()) { std::bad_alloc{}; }
        clear();
        iterator_assign(first, last);
    }
    void assign(size_type n, const_reference u) {
        clear();
        resize(n, u);
    }
    void assign(std::initializer_list<value_type> il) {
        if (std::distance(il.begin(), il.end()) > max_size()) { std::bad_alloc{}; }
        clear();
        iterator_assign(il.begin(), il.end());
    }

    reference push_back(const_reference x) {
        if (size() == capacity()) { throw std::bad_alloc{}; }
        return *(assign_value(data() + len++, x));
    }
    reference push_back(value_type&& x) {
        if (size() == capacity()) { throw std::bad_alloc{}; }
        return *(assign_value(data() + len++, std::move(x)));
    }

    template <typename... Args>
    reference emplace_back(Args&&... args) {
        if (size() == capacity()) { throw std::bad_alloc{}; }
        return *(assign_value(data() + len++, std::forward<Args>(args)...));
    }

    void pop_back() {
        assert(empty() == false);
        std::destroy_at(back());
        --len;
    }

    iterator insert(const_iterator position, const_reference x) {
        iterator pos = const_cast<iterator>(position);
        if (size() == capacity()) { throw std::bad_alloc{}; }
        insert_shift_right(pos, end(), 1);
        assign_value(pos, x);
        ++len;
        return pos;
    }
    iterator insert(const_iterator position, value_type&& x) {
        iterator pos = const_cast<iterator>(position);
        if (size() == capacity()) { throw std::bad_alloc{}; }
        insert_shift_right(pos, end(), 1);
        assign_value(pos, std::move(x));
        ++len;
        return pos;
    }
    iterator insert(const_iterator position, size_type n, const_reference x) {
        if (size() + n > capacity()) { throw std::bad_alloc{}; }
        iterator pos   = const_cast<iterator>(position);
        auto shift_end = insert_shift_right(pos, end(), n);
        for (auto it = pos; it != shift_end; ++it) {
            assign_value(pos, x);
        }
        return pos;
    }
    template <typename InputIterator>
    iterator insert(const_iterator position, InputIterator first, InputIterator last) {
        auto diff    = std::distance(first, last);
        iterator pos = const_cast<iterator>(position);
        if (size() + diff > capacity()) { throw std::bad_alloc{}; }
        auto shift_end = insert_shift_right(pos, end(), diff);
        for (auto it = pos; it != shift_end; ++it) {
            assign_value(it, *first++);
        }
        return pos;
    }
    iterator insert(const_iterator position, std::initializer_list<value_type> il) { return insert(position, il.begin(), il.end()); }

    template <typename... Args>
    iterator emplace(const_iterator position, Args&&... args) {
        iterator pos = const_cast<iterator>(position);
        if (size() == capacity()) { throw std::bad_alloc{}; }
        insert_shift_right(pos, end(), 1);
        assign_value(pos, std::forward<Args>(args)...);
        ++len;
        return pos;
    }

    iterator erase(const_iterator position) {
        iterator pos = const_cast<iterator>(position);
        std::destroy_at(pos);
        erase_shift_left(pos, end(), 1);
        --len;
        return pos;
    }
    iterator erase(const_iterator first, const_iterator last) {
        iterator fs = const_cast<iterator>(first);
        iterator ls = const_cast<iterator>(last);
        for (auto it = fs; it != ls; ++it) {
            std::destroy_at(it);
        }
        len -= std::distance(fs, ls);
        erase_shift_left(fs, end(), std::distance(fs, ls));
        return fs;
    }

    void swap(fixed_capacity_vector& x);

    void clear() noexcept {
        for (auto it = begin(); it != end(); ++it) { std::destroy_at(it); }
        len = 0;
    }

private:
    static pointer assign_value(pointer ptr) {
        return new (ptr) value_type{};
    }
    template <typename... Args>
    static pointer assign_value(pointer ptr, Args&&... args) {
        return new (ptr) value_type(std::forward<Args>(args)...);
    }

    template <typename InputIterator>
    void iterator_assign(InputIterator first, InputIterator last) {
        for (; first != last; ++first) { assign_value(data() + len++, *first); }
    }

    static iterator erase_shift_left(iterator first, iterator last, size_type n) {
        for (; first != last; ++first) {
            assign_value(first, std::move(*(std::next(first, n))));
        }
        return last - n;
    }
    static iterator insert_shift_right(iterator first, iterator last, size_type n) {
        for (auto it = last - 1; it >= first; --it) {
            assign_value(std::next(it, n), std::move(*it));
            std::destroy_at(it);
        }
        return last;
    }
};

template <typename T>
class fixed_capacity_vector<T, 0> {};