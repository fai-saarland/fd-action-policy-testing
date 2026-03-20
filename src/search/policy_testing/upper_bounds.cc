#include <algorithm>

#include "upper_bounds.h"
#include "../task_utils/task_properties.h"

namespace policy_testing {
UpperBoundsExtended::UpperBoundsExtended(int num_policies, bool parent_propagation, bool register_unsolved)
    : parent_propagation(parent_propagation),
      upper_bounds(Policy::UNSOLVED),
      register_unsolved(register_unsolved),
      policy_cost_cache(std::vector<PolicyCost>(num_policies, Policy::UNKNOWN)),
      cached_parents(std::vector<std::pair<State, int>>(num_policies)) {
    // State default constructor: StateID -1
}

BoundResult UpperBoundsExtended::run_plan(const TaskProxy &task_proxy, const State &state, std::vector<OperatorID> plan, int model) {
    (void)task_proxy;
    (void)state;
    (void)plan;
    (void)model;
    return BoundResult {-1, Policy::UNKNOWN, Policy::UNKNOWN};
}

BoundResult UpperBoundsExtended::run_policy(const TaskProxy &task_proxy, const State &state, RemotePolicy &remote_policy, int model) {
    if (model != -1 && policy_cost_cache[state][model] != Policy::UNKNOWN) {
        // Policy ran before already; take upper bound immediately.
        return BoundResult {model, policy_cost_cache[state][model], upper_bounds[state]};
    }

    std::vector<OperatorID> plan;
    std::vector<State> path;
    // const auto [result_known, solved] = remote_policy.execute_get_plan_and_path(state, plan, path, model);
    const auto [result_known, solved] = remote_policy.execute_get_transitions_and_path(state, plan, path, model);

    // Store path for later propagation, if enabled, even if unsolved.
    if (solved || register_unsolved) {
        if (model != -1 && parent_propagation) {
            register_path(task_proxy, plan, path, model);
            // assert(false);
        }
    }

    if (!result_known) {
        // Did not finish execution; e.g. cutoff
        return BoundResult {model, Policy::UNKNOWN, Policy::UNKNOWN};
    }

    if (!solved && !register_unsolved) {
        if (model != -1) {
            for (const State &state : path) {
                if (policy_cost_cache[state][model] == Policy::UNKNOWN) {
                    policy_cost_cache[state][model] = Policy::UNSOLVED;
                }
            }
        }
        return BoundResult {model, Policy::UNSOLVED, upper_bounds[state]};
    }

    int acc_cost = Policy::UNSOLVED;
    int curr_policy_cost = Policy::UNSOLVED;
    if (solved) {
        acc_cost = 0;
        curr_policy_cost = 0;
    }
    assert((plan.size() == path.size() - 1 && solved) || (plan.size() == path.size() && !solved));
    for (int i = path.size() - 1; i >= 0; --i) {
        State curr_state = path[i];
        assert((task_properties::is_goal_state(task_proxy, curr_state) || !solved) || acc_cost != 0);
        // Operator corresponding to state is at i - 1, regardless of whether path.size = plan.size (+ 1)
        OperatorID curr_op = i > 0 ? plan[i - 1] : Policy::NO_OPERATOR;

        if (model != -1 && policy_cost_cache[curr_state][model] == Policy::UNKNOWN) {
            policy_cost_cache[curr_state][model] = curr_policy_cost;
        }
        if (!update_bound(curr_state, acc_cost)) {
            acc_cost = upper_bounds[curr_state];
        }
        if (curr_op != Policy::NO_OPERATOR) {
            if (acc_cost >= 0) {
                acc_cost += task_proxy.get_operators()[curr_op.get_index()].get_cost();
            }
            if (curr_policy_cost >= 0) {
                // curr_policy_cost will only ever increase for solved; this is intentional
                curr_policy_cost += task_proxy.get_operators()[curr_op.get_index()].get_cost();
            }
        } else {
            assert(curr_state == state);
        }
    }
    if (parent_propagation) {
        propagate_bounds();
    }
    return BoundResult {model, curr_policy_cost, upper_bounds[state]};
}

bool UpperBoundsExtended::inject_upper_bound(State &state, int new_bound) {
    bool result = update_bound(state, new_bound);
    // TODO: store as bound update (if applicable)
    // TODO: initiate bound propagation
    return result;
}

void UpperBoundsExtended::register_subsequent_bug_reporting(PolicyTestingBaseEngine *engine, const State &pool_state, int policy_value) {
    subsequent_bug_reporting.insert({pool_state.get_id(), [engine, pool_state, policy_value](int new_value) {
                                         if (engine->is_known_bug(pool_state)) {
                                             return;
                                         }
                                         if (policy_value == Policy::UNSOLVED) {
                                             engine->add_additional_bug(pool_state, TestResult(UNSOLVED_BUG_VALUE, new_value));
                                             return;
                                         }
                                         engine->add_additional_bug(pool_state, TestResult {policy_value - new_value, new_value});
                                     }
                                    });
}

// PRIVATE //

void UpperBoundsExtended::propagate_bounds(int model) {
    if (!parent_propagation) {
        return;
    }

    while (!improved_bounds.empty()) {
        std::tuple<State, int, int> improved_bound = improved_bounds.back();
        improved_bounds.pop_back();

        State improved_state = std::get<0>(improved_bound);
        int new_bound = std::get<2>(improved_bound);

        std::vector<std::pair<State, int>> parents = cached_parents[improved_state];

        // If model set (!= -1), just update parent at model idx, otherwise all parents.
        unsigned long from = model == -1 ? 0 : model;
        unsigned long to = model == -1 ? parents.size() - 1 : model;
        for (unsigned long i = from; i <= to; ++i) {
            auto [parent, cost] = parents[i];
            if (!parent.get_registry()) {
                continue;
            }

            int old_parent_cost = upper_bounds[parent];
            int new_parent_cost = new_bound + cost;
            if (update_bound(parent, new_parent_cost)) {
                improved_bounds.emplace_back(parent, old_parent_cost, new_parent_cost);
            }
        }
    }
}

bool UpperBoundsExtended::update_bound(State &state, PolicyCost new_cost) {
    PolicyCost old_cost = upper_bounds[state];
    if (Policy::is_less(new_cost, old_cost)) {
        if (parent_propagation) {
            improved_bounds.emplace_back(state, old_cost, new_cost);
        }
        if (subsequent_bug_reporting.contains(state.get_id())) {
            subsequent_bug_reporting.at(state.get_id())(new_cost);
            subsequent_bug_reporting.erase(state.get_id());
        }
        if (stateIsInCostSet(state, old_cost)) { // TODO: rather in tested_states? or is stateIsInCostSet(s) <=> s in tested_states?
            delayed_cost_set_updates.emplace_back(state, old_cost, new_cost);
        }
        upper_bounds[state] = new_cost;
        return true;
    }
    return false;
}

void UpperBoundsExtended::register_path(const TaskProxy &task_proxy, std::vector<OperatorID> &plan, std::vector<State> &path, int model) {
    assert(plan.size() == path.size() - 1 || plan.size() == path.size()); // Plan is same size (unsolved) or one shorter (solved)
    for (int i = path.size() - 1; i > 0; --i) {
        State curr_state = path[i];
        State prev_state = path[i - 1];
        OperatorID curr_op = plan[i - 1];

        cached_parents[curr_state][model] = std::make_pair(prev_state, task_proxy.get_operators()[curr_op.get_index()].get_cost());
    }
}
}
