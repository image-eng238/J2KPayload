#pragma once
#include <chrono>
#include <initializer_list>

class j2k_measure {
public:
    using clock_t = std::chrono::high_resolution_clock;
    using get_dft = std::chrono::milliseconds;

    static clock_t::time_point tic_for(std::initializer_list<j2k_measure*> ls, const clock_t::time_point& t = clock_t::now()) {
        for (auto m : ls) { m->tic(t); }
        return t;
    }
    static clock_t::time_point toc_for(std::initializer_list<j2k_measure*> ls, const clock_t::time_point& t = clock_t::now()) {
        for (auto m : ls) { m->toc(t); }
        return t;
    }
    static clock_t::time_point toc_push_for(std::initializer_list<j2k_measure*> ls, const clock_t::time_point& t = clock_t::now()) {
        for (auto m : ls) { m->toc_push(t); }
        return t;
    }
    static void reset_for(std::initializer_list<j2k_measure*> ls) {
        for (auto m : ls) { m->reset(); }
    }

private:
    clock_t::time_point point;
    clock_t::duration sum;
    size_t count;

public:
    j2k_measure() = default;
    clock_t::time_point tic(const clock_t::time_point& t = clock_t::now()) {
        return point = t;
    }

    clock_t::time_point toc(const clock_t::time_point& t = clock_t::now()) {
        sum   = t - point;
        count = 1;
        return t;
    }

    clock_t::time_point toc_push(const clock_t::time_point& t = clock_t::now()) {
        sum += t - point;
        count += 1;
        return t;
    }

    void reset() {
        *this = {};
    }

    template <typename T = get_dft, typename U = double>
    U get() const {
        return std::chrono::duration_cast<std::chrono::duration<U, typename T::period>>(sum).count();
    }

    template <typename T = get_dft, typename U = double>
    U average() const {
        return average<T, U>(count);
    }

    template <typename T = get_dft, typename U = double>
    U average(size_t n) const {
        if (n) {
            return get<T, U>() / n;
        } else {
            return get<T, U>();
        }
    }
};
