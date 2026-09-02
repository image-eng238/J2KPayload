#include "measure.hpp"

#include <iostream>
#include <algorithm>
#include <numeric>
#include <cassert>

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

    {
        int data[] = {
            2350, 397, 15206, 20312, 19367, 2944, 26334, 11816,
            13174, 13207, 32749, 3206, 2892, 27693, 31990, 7358,
            22721, 14651, 14017, 12177, 22985, 615, 21932, 11992,
            22477, 3221, 10031, 25561, 19553, 13059, 27781, 21904,
            13456, 10219, 9448, 55, 13163, 3014, 11871, 26337, 16221,
            11853, 29544, 19113, 6778, 28766, 26471, 29499, 10649, 7720
        };
        const auto n  = sizeof(data) / sizeof(data[0]);
        auto sum      = std::accumulate<int*, size_t>(std::begin(data), std::end(data), 0);
        auto avg      = static_cast<double>(sum) / n;
        double stddev = 0;
        {
            double M2 = 0;
            for (auto e : data) {
                double d = (e - avg);
                M2 += d * d;
            }
            stddev = sqrt(M2 / n);
        }

        j2k_stats<int> stats_test;
        stats_test.add_sample_for(data);
        assert(n == stats_test.sample_size());
        assert(avg == stats_test.avarage());
        assert(*std::min_element(std::begin(data), std::end(data)) == stats_test.minimum());
        assert(*std::max_element(std::begin(data), std::end(data)) == stats_test.maximum());
        auto stddev_2 = stats_test.stddev();
        printf("stddev=%lf, %lf\n", stddev, stddev_2);
    }

    return 0;
}