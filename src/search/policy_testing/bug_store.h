#pragma once

#include "../state_registry.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class AbstractTask;

namespace policy_testing {
using BugValue = int;

inline constexpr BugValue NOT_APPLICABLE_INDICATOR = -1;
inline constexpr BugValue UNSOLVED_BUG_VALUE = std::numeric_limits<int>::max();

struct BugStoreEntry {
    State state;
    BugValue bug_value;

    explicit BugStoreEntry(State s, const BugValue &v)
        : state(std::move(s)), bug_value(v) {
    }
};

using BugStore = std::vector<BugStoreEntry>;

class BugStoreFile {
public:
    explicit BugStoreFile(
        const std::shared_ptr<AbstractTask> &task,
        const std::string &path);

    ~BugStoreFile() = default;

    void write(const State &state, const BugValue &bug_value);

    void write(const BugStoreEntry &entry) {
        write(entry.state, entry.bug_value);
    }

    BugStoreFile &operator<<(const BugStoreEntry &entry) {
        write(entry);
        return *this;
    }

private:
    std::ofstream out_;
};

BugStore load_bug_file(
    const std::shared_ptr<AbstractTask> &task,
    StateRegistry &state_registry,
    const std::string &path);

BugStore load_bugs(
    const std::shared_ptr<AbstractTask> &task,
    StateRegistry & state_registry,
    std::istream &in);

BugStore parse_bugs(
    const std::shared_ptr<AbstractTask> &task,
    StateRegistry & state_registry,
    std::istream &in);
} // namespace policy_testing
