#include "fixed_capacity_vector.hpp"

int main(void) {
    fixed_capacity_vector<char, 8> vec;
    vec.push_back('a');
    vec.erase(vec.begin(), vec.end() - 1);

    return 0;
}