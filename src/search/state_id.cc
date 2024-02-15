#include "state_id.h"
#include "utils/hash.h"

#include <ostream>

using namespace std;

const StateID StateID::no_state = StateID(-1);

ostream &operator<<(ostream &os, StateID id) {
    os << "#" << id.value;
    return os;
}

void StateID::feed_to_hash_state(utils::HashState &hash_state) const {
    hash_state.feed(value);
}
