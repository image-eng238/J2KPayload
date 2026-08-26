#include "measure.hpp"

#include <iostream>

int main() {
    using test_clock_t = std::chrono::milliseconds;
    j2k_measure m, mm;
    auto s = m.tic();
    auto e = m.toc();

    mm.tic();
    auto test_time = std::chrono::duration_cast<test_clock_t>(e - s).count();
    auto r         = m.average<test_clock_t>();

    mm.toc();
    r = m.average<test_clock_t>();

    j2k_measure::reset_for({&m, &mm});
    j2k_measure::reset_for({&m, &mm});

    return 0;
}