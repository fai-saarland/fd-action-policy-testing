#pragma once

#include <exception>

namespace policy_testing {
class OutOfResourceException : public std::exception {
};
}
