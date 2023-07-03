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
struct PoolEntry {
    /** Back reference to state in pool used to generate this entry. **/
    StateID ref_state;
    /** Index of the ref state in the pool upon generation of this pool entry **/
    int ref_index;
    /** Number of fuzzing operations applied to the back-referenced state. **/
    int steps;
    /** The actual pool state of this entry */
    State state;

    PoolEntry(int ref_index, StateID ref, int steps, State s)
        : ref_state(ref), ref_index(ref_index), steps(steps), state(std::move(s)) {
    }

    PoolEntry(int ref_index, int steps, State s, const std::vector<PoolEntry> &pool)
        : ref_state(ref_index < 0 ? StateID::no_state : pool[ref_index].state.get_id())
          , ref_index(ref_index)
          , steps(steps),
          state(std::move(s)) {
    }
};

using Pool = std::vector<PoolEntry>;

class PoolFile {
public:
    explicit PoolFile(
        const std::shared_ptr<AbstractTask> &task,
        const std::string &path);
    ~PoolFile() = default;

    void write(int ref_index, int steps, const State &state);
    void write(const PoolEntry &entry);

private:
    std::ofstream out_;
};

Pool load_pool_file(
    const std::shared_ptr<AbstractTask> &task,
    StateRegistry &state_registry,
    const std::string &path);

Pool load_pool(
    const std::shared_ptr<AbstractTask> &task,
    StateRegistry & state_registry,
    std::istream &in);

Pool parse_pool_entries(
    const std::shared_ptr<AbstractTask> &task,
    StateRegistry & state_registry,
    std::istream &in);
} // namespace policy_testing
