#pragma once

#include <cstdlib>
#include <ostream>
#include <utility>
#include <vector>
#include <algorithm>
#include <iostream>

namespace simulations {
template<class T>
extern bool is_sorted_unique(const std::vector<T> &values) {
    for (size_t i = 1; i < values.size(); ++i) {
        if (values[i - 1] >= values[i])
            return false;
    }
    return true;
}

template<class Sequence>
size_t hash_number_sequence(const Sequence &data, size_t length) {
    // hash function adapted from Python's hash function for tuples.
    size_t hash_value = 0x345678;
    size_t mult = 1000003;
    for (int i = length - 1; i >= 0; --i) {
        hash_value = (hash_value ^ data[i]) * mult;
        mult += 82520 + i + i;
    }
    hash_value += 97531;
    return hash_value;
}

template<typename T, typename F>
void erase_if(T &container, F lambda) {
    container.erase(std::remove_if(container.begin(), container.end(), lambda), container.end());
}
}
