#ifndef STATE_ID_H
#define STATE_ID_H

#include <iostream>
#include "utils/hash.h"

// For documentation on classes relevant to storing and working with registered
// states see the file state_registry.h.

class StateID;

namespace utils {
void feed(HashState &, StateID);
}

class StateID {
    friend class StateRegistry;
    friend std::ostream &operator<<(std::ostream &os, StateID id);
    template<typename>
    friend class PerStateInformation;
    template<typename>
    friend class PerStateArray;
    friend class PerStateBitset;
    friend void utils::feed(utils::HashState &, StateID);

    int value;

public:
    explicit StateID(int value_)
        : value(value_) {
    }
    // No implementation to prevent default construction
    StateID() = delete;
    ~StateID() = default;

    static const StateID no_state;

    [[nodiscard]] int get_value() const {
        return value;
    }

    bool operator==(const StateID &other) const {
        return value == other.value;
    }

    bool operator!=(const StateID &other) const {
        return !(*this == other);
    }

    std::strong_ordering operator<=>(const StateID &other) const {
        return value <=> other.value;
    }
};

namespace utils {
void
feed(HashState &hash_state, StateID sid) {
    feed(hash_state, sid.value);
}
}

#endif
