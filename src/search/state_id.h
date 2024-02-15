#ifndef STATE_ID_H
#define STATE_ID_H

#include <iostream>

namespace utils {
class HashState;
}


// For documentation on classes relevant to storing and working with registered
// states see the file state_registry.h.

class StateID {
    friend class StateRegistry;
    friend std::ostream &operator<<(std::ostream &os, StateID id);
    template<typename>
    friend class PerStateInformation;
    template<typename>
    friend class PerStateArray;
    friend class PerStateBitset;

    int value;
    explicit StateID(int value_)
        : value(value_) {
    }

    // No implementation to prevent default construction
    StateID();
public:
    ~StateID() {
    }

    static const StateID no_state;

    bool operator==(const StateID &other) const {
        return value == other.value;
    }

    bool operator!=(const StateID &other) const {
        return !(*this == other);
    }

    std::strong_ordering operator<=>(const StateID &other) const {
        return value <=> other.value;
    }

    explicit operator std::string() const {
        return std::to_string(value);
    }

    void feed_to_hash_state(utils::HashState &hash_state) const;
};

namespace utils {
inline void feed(HashState &hash_state, StateID id) {
    id.feed_to_hash_state(hash_state);
}
}

#endif
