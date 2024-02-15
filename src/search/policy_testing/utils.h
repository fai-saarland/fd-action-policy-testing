#pragma once

#include "../abstract_task.h"
#include "../operator_id.h"
#include "../utils/rng.h"

#include <memory>
#include <utility>
#include <vector>
#include <optional>

class State;

namespace policy_testing {
template<typename Plan>
inline int calculate_plan_cost(const std::shared_ptr<AbstractTask> &task, const Plan &plan) {
    int res = 0;
    for (const OperatorID &op_id : plan) {
        res += task->get_operator_cost(op_id.get_index(), false);
    }
    return res;
}

std::shared_ptr<AbstractTask> get_modified_initial_state_task(
    const std::shared_ptr<AbstractTask> &base_task,
    const State &new_state);

std::shared_ptr<AbstractTask> get_modified_initial_state_and_goal_task(
    const std::shared_ptr<AbstractTask> &base_task,
    const State &new_start_state,
    const State &new_goal_state);

inline std::shared_ptr<AbstractTask> get_modified_task(
    const std::shared_ptr<AbstractTask> &base_task,
    const State &new_start_state,
    const State *new_goal_state) {
    if (new_goal_state) {
        return get_modified_initial_state_and_goal_task(base_task, new_start_state, *new_goal_state);
    } else {
        return get_modified_initial_state_task(base_task, new_start_state);
    }
}

std::shared_ptr<AbstractTask> get_modified_initial_state_task(
    const std::shared_ptr<AbstractTask> &base_task,
    const std::vector<int> &new_state_values);

bool verify_plan(
    const std::shared_ptr<AbstractTask> &task,
    const State &state,
    const std::vector<OperatorID> &plan);

using timestamp_t = long long;

timestamp_t get_timestamp();
timestamp_t get_end_timestamp(timestamp_t max_time_seconds);
} // namespace policy_testing
