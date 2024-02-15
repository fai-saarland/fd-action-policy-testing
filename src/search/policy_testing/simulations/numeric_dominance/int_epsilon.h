#pragma once

#include <cassert>
#include <iostream>

namespace simulations {
class IntEpsilon {
    int value;
    int epsilon;

public:
    IntEpsilon() : value(0), epsilon(0) {}

    IntEpsilon(int value_) : value(value_), epsilon(0) {
    }

    IntEpsilon(int value_, int epsilon_) : value(value_), epsilon(epsilon_) {
        assert(epsilon >= -1 && epsilon <= 1);
    }

    IntEpsilon(const IntEpsilon &) = default;

    IntEpsilon &operator=(const IntEpsilon &) = default;

    IntEpsilon(IntEpsilon &&) = default;

    IntEpsilon &operator=(IntEpsilon &&) = default;

    ~IntEpsilon() = default;

    bool operator<(const IntEpsilon &other) const;

    bool operator<=(const IntEpsilon &other) const;

    bool operator==(const IntEpsilon &other) const;

    bool operator!=(const IntEpsilon &other) const;

    bool operator>(const IntEpsilon &other) const;

    bool operator>=(const IntEpsilon &other) const;

    IntEpsilon &operator+=(const IntEpsilon &other);

    IntEpsilon &operator+=(const int &other);

    IntEpsilon &operator-=(const int &other);

    IntEpsilon &operator-=(const IntEpsilon &other);

    IntEpsilon operator-() const;

    template<typename T>
    const IntEpsilon operator-(const T &other) const {
        return IntEpsilon(*this) -= other;
    }

    template<typename T>
    const IntEpsilon operator+(const T &other) const {
        return IntEpsilon(*this) += other;
    }

    int get_value() const {
        return value;
    }

    int get_epsilon() const {
        assert(epsilon >= -1 && epsilon <= 1);
        return epsilon;
    }

    friend std::ostream &operator<<(std::ostream &os, const IntEpsilon &other);
};


class IntEpsilonSum {
    int value;
    int epsilon;

public:
    IntEpsilonSum(int value_) : value(value_), epsilon(0) {
    }

    IntEpsilonSum(int value_, int epsilon_) : value(value_), epsilon(epsilon_) {}

    IntEpsilonSum(const IntEpsilonSum &) = default;

    IntEpsilonSum &operator=(const IntEpsilonSum &) = default;

    IntEpsilonSum(IntEpsilonSum &&) = default;

    IntEpsilonSum &operator=(IntEpsilonSum &&) = default;

    ~IntEpsilonSum() = default;

    bool operator<(const IntEpsilonSum &other) const;

    bool operator<=(const IntEpsilonSum &other) const;

    bool operator==(const IntEpsilonSum &other) const;

    bool operator!=(const IntEpsilonSum &other) const;

    bool operator>(const IntEpsilonSum &other) const;

    bool operator>=(const IntEpsilonSum &other) const;

    IntEpsilonSum &operator+=(const IntEpsilonSum &other);

    IntEpsilonSum &operator-=(const IntEpsilonSum &other);


    template<typename T>
    const IntEpsilonSum operator+(const T &other) const {
        return IntEpsilonSum(*this) += other;
    }

    template<typename T>
    const IntEpsilonSum operator-(const T &other) const {
        return IntEpsilonSum(*this) -= other;
    }

    IntEpsilon get_epsilon_negative() const;

    friend std::ostream &operator<<(std::ostream &os, const IntEpsilonSum &other);
};


template<typename T>
inline T get_epsilon() {
    return T(0);
}

template<>
inline IntEpsilon get_epsilon() {
    return IntEpsilon(0, 1);
}

template<>
inline IntEpsilonSum get_epsilon() {
    return IntEpsilonSum(0, 1);
}

template<typename T>
inline T epsilon_if_zero(T t) {
    if (t == T(0)) {
        return get_epsilon<T>();
    }
    return t;
}
}
