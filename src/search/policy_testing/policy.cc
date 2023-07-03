#include "policy.h"

#include "../option_parser.h"
#include "../plugin.h"
#include "../task_utils/task_properties.h"
#include "out_of_resource_exception.h"
#include "../evaluation_result.h"
#include "../evaluation_context.h"
#include "../evaluator.h"

#include <cassert>
#include <vector>
#include <iostream>
#include <string>

#include <boost/iostreams/filtering_streambuf.hpp>
#include <ranges>

namespace policy_testing {
Policy::Policy(const options::Options &opts)
    : TestingBaseComponent(),
      operator_cache_(NO_CACHED_OPERATOR),
      policy_cost_cache_(UNKNOWN),
      steps_limit(opts.get<unsigned int>("steps_limit")) {
}

void
Policy::store_operator(const State &state, const OperatorID &op_id) {
    // assume deterministic policy:
    assert(operator_cache_[state] == NO_CACHED_OPERATOR || operator_cache_[state] == op_id.get_index());
    assert(op_id == NO_OPERATOR || task_properties::is_applicable(get_task_proxy().get_operators()[op_id], state));
    operator_cache_[state] = op_id.get_index();
}

OperatorID
Policy::lookup_apply(const State &state) {
    int &op_cache = operator_cache_[state];
    if (op_cache == NO_CACHED_OPERATOR) {
        const OperatorID op = apply(state);
        assert(op == NO_OPERATOR || task_properties::is_applicable(get_task_proxy().get_operators()[op], state));
        const int op_index = op.get_index();
        op_cache = op_index;
        if (running_cache_writer) {
            running_cache_writer->write(state, op_index);
        }
        if (op != NO_OPERATOR) {
            const OperatorProxy &op_proxy = get_task_proxy().get_operators()[op];
            State succ = get_state_registry().get_successor_state(state, op_proxy);
            insert_sorted(policy_parent[succ], state.get_id());
        }
    }
    return OperatorID(op_cache);
}

int Policy::read_action_cost(const State &s) const {
    assert(can_lookup_action(s));
    OperatorID op = lookup_action(s);
    assert(op != NO_OPERATOR);
    return get_operator_cost(op);
}

std::vector<int> Policy::read_path_action_costs(const std::vector<State> &path) const {
    std::vector<int> action_costs;
    if (path.size() > 1) {
        for (const State &s: path | std::ranges::views::take(path.size() - 1)) {
            action_costs.push_back(read_action_cost(s));
        }
    }
    assert(action_costs.size() + 1 == path.size());
    return action_costs;
}

Policy::RunResult
Policy::execute_get_plan(const State &state0, std::vector<OperatorID> &plan,
                         std::optional<unsigned int> step_limit_override) {
    assert(plan.empty());
    const unsigned int step_limit_local = step_limit_override.value_or(steps_limit);
    utils::HashSet<StateID> seen;
    seen.insert(state0.get_id());
    State state = state0;
    for (unsigned int step = 0;; ++step) {
        if (task_properties::is_goal_state(get_task_proxy(), state)) {
            return {true, true};
        }
        if (are_limits_reached()) {
            throw OutOfResourceException();
        }
        bool may_execute = step < step_limit_local || !step_limit_local;
        if (!can_lookup_action(state) && !may_execute) {
            break;
        }
        OperatorID op = lookup_apply(state);
        if (op == NO_OPERATOR) {
            plan.clear();
            return {true, false};
        }
        plan.push_back(op);
        state = get_successor_state(state, op);
        if (!seen.insert(state.get_id()).second) {
            plan.clear();
            return {true, false};
        }
    }
    plan.clear();
    return {false, false};
}

Policy::RunResult
Policy::execute_get_plan(const State &state0, std::vector<OperatorID> &plan, PolicyCost max_cost, int max_steps,
                         const std::shared_ptr<Evaluator> &dead_end_evaluator) {
    assert(plan.empty());

    const bool cost_limit_set = max_cost >= 0;
    const bool step_limit_set = max_steps >= 0;

    utils::HashSet<StateID> seen;
    seen.insert(state0.get_id());
    State state = state0;
    PolicyCost current_cost = 0;

    for (int step_counter = 0;
         (!cost_limit_set || current_cost < max_cost) && (!step_limit_set || step_counter <= max_steps);
         ++step_counter) {
        if (task_properties::is_goal_state(get_task_proxy(), state)) {
            return {true, true};
        }
        if (dead_end_evaluator) {
            EvaluationContext ctxt(state);
            EvaluationResult res = dead_end_evaluator->compute_result(ctxt);
            if (res.is_infinite()) {
                plan.clear();
                return {true, false};
            }
        }
        if (are_limits_reached()) {
            throw OutOfResourceException();
        }
        OperatorID op = lookup_apply(state);
        if (op == NO_OPERATOR) {
            plan.clear();
            return {true, false};
        }
        plan.push_back(op);
        state = get_successor_state(state, op);
        if (!seen.insert(state.get_id()).second) {
            plan.clear();
            return {true, false};
        }
        current_cost += get_operator_cost(op);
    }
    plan.clear();
    return {false, false};
}

Policy::RunResult
Policy::execute_get_plan_and_path(const State &state0, std::vector<OperatorID> &plan, std::vector<State> &path,
                                  std::optional<unsigned int> step_limit_override,
                                  bool continue_with_cached_actions) {
    assert(plan.empty());
    assert(path.empty());

    const unsigned int step_limit_local = step_limit_override.value_or(steps_limit);
    utils::HashSet<StateID> seen;
    seen.insert(state0.get_id());
    State state = state0;
    for (unsigned int step = 0;; ++step) {
        path.emplace_back(state);
        if (task_properties::is_goal_state(get_task_proxy(), state)) {
            return {true, true};
        }
        if (are_limits_reached()) {
            throw OutOfResourceException();
        }
        bool may_execute = step < step_limit_local || !step_limit_local;
        if (!(continue_with_cached_actions && can_lookup_action(state)) && !may_execute) {
            break;
        }
        OperatorID op = lookup_apply(state);
        if (op == NO_OPERATOR) {
            plan.clear();
            return {true, false};
        }
        plan.push_back(op);
        state = get_successor_state(state, op);
        if (!seen.insert(state.get_id()).second) {
            plan.clear();
            return {true, false};
        }
    }
    plan.clear();
    return {false, false};
}

bool
Policy::has_complete_cached_path(const State &state0) {
    utils::HashSet<StateID> seen;
    seen.insert(state0.get_id());
    State state = state0;
    while (true) {
        if (task_properties::is_goal_state(get_task_proxy(), state)) {
            return true;
        }
        if (!can_lookup_action(state)) {
            return false;
        }
        OperatorID op = lookup_apply(state);
        if (op == NO_OPERATOR) {
            break;
        }
        state = get_successor_state(state, op);
        if (!seen.insert(state.get_id()).second) {
            break;
        }
    }
    return true;
}

std::vector<State> Policy::execute_get_path_fragment(const State &state0,
                                                     std::optional<unsigned int> step_limit_override,
                                                     bool continue_with_cached_actions) {
    utils::HashSet<StateID> seen;
    std::vector<State> path;
    seen.insert(state0.get_id());
    State state = state0;
    unsigned int local_steps_limit = step_limit_override.value_or(steps_limit);
    for (unsigned int step = 0;; ++step) {
        bool may_execute = step < local_steps_limit || !local_steps_limit;
        path.emplace_back(state);
        if (task_properties::is_goal_state(get_task_proxy(), state)) {
            assert(!path.empty());
            return path;
        }
        if (are_limits_reached()) {
            throw OutOfResourceException();
        }
        if (!(continue_with_cached_actions && can_lookup_action(state)) && !may_execute) {
            break;
        }
        OperatorID op = lookup_apply(state);
        if (op == NO_OPERATOR) {
            break;
        }
        state = get_successor_state(state, op);
        if (!seen.insert(state.get_id()).second) {
            break;
        }
    }
    assert(!path.empty());
    return path;
}

PolicyCost
Policy::get_complete_policy_cost(const State &state) {
    std::vector<OperatorID> plan;
    const auto [result_valid, solved] = execute_get_plan(state, plan, 0);
    assert(result_valid);
    if (solved) {
        return calculate_plan_cost(get_task(), plan);
    }
    return UNSOLVED;
}

PolicyCost Policy::get_operator_cost(OperatorID op) const {
    return get_task()->get_operator_cost(op.get_index(), false);
}

bool Policy::is_goal(const State &state) const {
    return task_properties::is_goal_state(get_task_proxy(), state);
}

PolicyCost
Policy::compute_policy_cost(const State &state, std::optional<unsigned int> step_limit_override,
                            bool continue_with_cached_actions) {
    PolicyCost &cost_cache_start = policy_cost_cache_[state];
    if (cost_cache_start == UNKNOWN) {
        // calculate policy cost
        std::vector<OperatorID> plan;
        std::vector<State> path;
        const auto [result_known, solved] = execute_get_plan_and_path(state, plan, path, step_limit_override,
                                                                      continue_with_cached_actions);
        if (!result_known) {
            return UNKNOWN;
        }
        PolicyCost remaining_cost = solved ? calculate_plan_cost(get_task(), plan) : UNSOLVED;
        cost_cache_start = remaining_cost;
        if (!plan.empty()) {
            for (unsigned int path_index = 1; path_index < path.size(); ++path_index) {
                // subtract cost from previous step
                if (remaining_cost != UNSOLVED) {
                    remaining_cost -= get_operator_cost(plan[path_index - 1]);
                }
                // update cost of intermediate state
                const auto &intermediate_state = path[path_index];
                PolicyCost &cost_cache_intermediate = policy_cost_cache_[intermediate_state];
                if (cost_cache_intermediate == UNKNOWN) {
                    cost_cache_intermediate = remaining_cost;
                } else {
                    assert(remaining_cost == cost_cache_intermediate);
                    break;
                }
            }
        }
    }
    return cost_cache_start;
}

std::pair<PolicyCost, bool>
Policy::compute_lower_policy_cost_bound(const State &s, std::optional<unsigned int> step_limit_override) {
    const PolicyCost base_cost = compute_policy_cost(s, step_limit_override);
    if (base_cost != UNKNOWN) {
        return {base_cost, true};
    }
    // policy cost is unknown compute lower bound without running policy
    State current_state = s;
    PolicyCost lower_cost_bound = 0;
    utils::HashSet<StateID> seen;
    seen.insert(current_state.get_id());
    while (true) {
        if (task_properties::is_goal_state(get_task_proxy(), current_state)) {
            assert(policy_cost_cache_[s] == UNKNOWN);
            policy_cost_cache_[s] = lower_cost_bound;
            return {lower_cost_bound, true};
        }
        if (!can_lookup_action(current_state)) {
            break;
        }
        OperatorID op = lookup_action(current_state);
        if (op == NO_OPERATOR) {
            assert(policy_cost_cache_[s] == UNKNOWN);
            policy_cost_cache_[s] = UNSOLVED;
            return {UNSOLVED, true};
        }
        lower_cost_bound += get_operator_cost(op);
        current_state = get_successor_state(current_state, op);
        if (!seen.insert(current_state.get_id()).second) {
            assert(policy_cost_cache_[s] == UNKNOWN);
            policy_cost_cache_[s] = UNSOLVED;
            return {UNSOLVED, true};
        }
    }
    return {lower_cost_bound, false};
}

std::pair<PolicyCost, bool>
Policy::read_lower_policy_cost_bound(const State &s) {
    const PolicyCost base_cost = policy_cost_cache_[s];
    if (base_cost != UNKNOWN) {
        return {base_cost, true};
    }
    // policy cost is unknown compute lower bound without running policy
    State current_state = s;
    PolicyCost lower_cost_bound = 0;
    utils::HashSet<StateID> seen;
    seen.insert(current_state.get_id());
    while (true) {
        if (task_properties::is_goal_state(get_task_proxy(), current_state)) {
            assert(policy_cost_cache_[s] == UNKNOWN);
            policy_cost_cache_[s] = lower_cost_bound;
            return {lower_cost_bound, true};
        }
        if (!can_lookup_action(current_state)) {
            break;
        }
        OperatorID op = lookup_action(current_state);
        if (op == NO_OPERATOR) {
            assert(policy_cost_cache_[s] == UNKNOWN);
            policy_cost_cache_[s] = UNSOLVED;
            return {UNSOLVED, true};
        }
        lower_cost_bound += get_operator_cost(op);
        current_state = get_successor_state(current_state, op);
        if (!seen.insert(current_state.get_id()).second) {
            assert(policy_cost_cache_[s] == UNKNOWN);
            policy_cost_cache_[s] = UNSOLVED;
            return {UNSOLVED, true};
        }
    }
    return {lower_cost_bound, false};
}

PolicyCost Policy::lazy_compute_policy_cost(const State &state,
                                            PolicyCost max_cost, int max_steps,
                                            const std::shared_ptr<Evaluator> &dead_end_evaluator) {
    std::vector<OperatorID> plan;
    const bool solved = execute_get_plan(state, plan, max_cost, max_steps, dead_end_evaluator).solves_state;
    return solved ? calculate_plan_cost(get_task(), plan) : UNSOLVED;
}

OperatorID
Policy::lookup_action(const State &state) const {
    assert(can_lookup_action(state));
    const int op_id = operator_cache_[state];
    return (op_id == NO_CACHED_OPERATOR) ? NO_OPERATOR : OperatorID(op_id);
}


void Policy::read_running_policy_cache(const std::string &cache_file) {
    std::ifstream istream(cache_file);
    const unsigned int state_size = get_task()->get_num_variables();
    for (std::string line; std::getline(istream, line);) {
        std::istringstream entry(line);
        int op;
        entry >> op;
        assert(op < get_task()->get_num_operators());
        std::vector<int> state_vec;
        state_vec.reserve(state_size);

        for (int val; entry >> val;) {
            state_vec.push_back(val);
        }

        State state = get_state_registry().insert_state(state_vec);
        operator_cache_[state] = op;

        if (OperatorID(op) != NO_OPERATOR) {
            assert(op >= 0);
            const OperatorProxy &op_proxy = get_task_proxy().get_operators()[op];
            State succ = get_state_registry().get_successor_state(state, op_proxy);
            insert_sorted(policy_parent[succ], state.get_id());
        }
    }
}

void Policy::add_options_to_parser(options::OptionParser &parser) {
    parser.add_option<unsigned int>("steps_limit",
                                    "The maximal number of steps to execute the policy. 0 means no limit",
                                    "0");
}

OperatorID CachedPolicy::apply(const State &) {
    std::cerr << "Cached policy can only read cached entries." << std::endl;
    utils::exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
}

static PluginTypePlugin<Policy> _type_plugin("PolicyForTesting", "", "policy");
static Plugin<Policy> _plugin("cached_policy", options::parse<Policy, CachedPolicy>);
} // namespace policy_testing
