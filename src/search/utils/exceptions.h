#ifndef UTILS_EXCEPTIONS_H
#define UTILS_EXCEPTIONS_H

#include <string>
#include <exception>

namespace utils {
// Base class for custom exception types.
class Exception : public std::exception {
protected:
    const std::string msg;
public:
    explicit Exception(const std::string &msg);
    ~Exception() override = default;

    [[nodiscard]] std::string get_message() const;
    virtual void print() const;
};
}

#endif
