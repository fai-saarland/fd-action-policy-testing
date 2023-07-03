#include "utils.h"

#include "../option_parser.h"
#include "../task_proxy.h"
#include "../task_utils/task_properties.h"
#include "../tasks/modified_init_goals_task.h"

#include <chrono>

namespace policy_testing {
std::shared_ptr<AbstractTask>
get_modified_initial_state_task(
    const std::shared_ptr<AbstractTask> &base_task,
    const std::vector<int> &new_state_values) {
    std::vector<FactPair> goal_facts;
    for (int i = 0; i < base_task->get_num_goals(); ++i) {
        goal_facts.push_back(base_task->get_goal_fact(i));
    }

    return std::make_shared<extra_tasks::ModifiedInitGoalsTask>(
        base_task, std::vector<int>(new_state_values), std::move(goal_facts));
}

std::shared_ptr<AbstractTask>
get_modified_initial_state_task(
    const std::shared_ptr<AbstractTask> &base_task,
    const State &new_state) {
    std::vector<int> values(new_state.size());
    for (unsigned i = 0; i < new_state.size(); ++i) {
        values[i] = new_state[i].get_value();
    }
    return get_modified_initial_state_task(base_task, values);
}

std::shared_ptr<AbstractTask> get_modified_initial_state_and_goal_task(
    const std::shared_ptr<AbstractTask> &base_task,
    const State &new_start_state,
    const State &new_goal_state) {
    // compute start state values
    std::vector<int> new_start_state_values(new_start_state.size());
    for (unsigned i = 0; i < new_start_state.size(); ++i) {
        new_start_state_values[i] = new_start_state[i].get_value();
    }
    // compute goal facts
    std::vector<FactPair> new_goal_facts;
    for (int i = 0; i < new_goal_state.size(); ++i) {
        new_goal_facts.emplace_back(i, new_goal_state[i].get_value());
    }
    return std::make_shared<extra_tasks::ModifiedInitGoalsTask>(
        base_task, std::move(new_start_state_values), std::move(new_goal_facts));
}

bool
verify_plan(
    const std::shared_ptr<AbstractTask> &base_task,
    const State &state0,
    const std::vector<OperatorID> &plan) {
    TaskProxy proxy(*base_task);
    State state = state0;
    state.unpack();
    for (auto i : plan) {
        if (!task_properties::is_applicable(
                proxy.get_operators()[i], state)) {
            return false;
        }
        state = state.get_unregistered_successor(proxy.get_operators()[i]);
    }
    return task_properties::is_goal_state(proxy, state);
}

timestamp_t
get_timestamp() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch())
           .count();
}

timestamp_t
get_end_timestamp(timestamp_t max_time) {
    return get_timestamp() + max_time;
}
} // namespace policy_testing
