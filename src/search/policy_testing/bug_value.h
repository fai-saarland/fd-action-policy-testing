#pragma once

#include <algorithm>
#include <limits>

namespace policy_testing {
using BugValue = int;
inline constexpr BugValue NOT_APPLICABLE_INDICATOR = -1;
inline constexpr BugValue UNSOLVED_BUG_VALUE = std::numeric_limits<int>::max();

BugValue bug_value_best_of(BugValue left, BugValue right) {
    if (left < 0) {
        return right;
    }
    if (right < 0) {
        return left;
    }
    return std::max(left, right);
}
}
