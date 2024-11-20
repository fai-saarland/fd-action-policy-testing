#pragma once

#include <exception>

namespace policy_testing {
// testing engine out of resource
class OutOfResourceException : public std::exception {
};

// decision to abstain from problem (because testing does not make any sense
// problem is too big, etc.)
class AbstentionException : public std::exception {
};
}
