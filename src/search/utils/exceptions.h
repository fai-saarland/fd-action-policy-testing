#ifndef UTILS_EXCEPTIONS_H
#define UTILS_EXCEPTIONS_H

#include <exception>

namespace utils {
// Base class for custom exception types.
class Exception : public std::exception {
public:
    ~Exception() override = default;
    virtual void print() const = 0;
};
}

#endif
