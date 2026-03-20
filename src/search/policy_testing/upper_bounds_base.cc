// #include <algorithm>

// #include "upper_bounds_base.h"
// #include "../task_utils/task_properties.h"

// namespace policy_testing {
// UpperBounds::UpperBounds(int num_policies, bool parent_propagation)
//     : parent_propagation(parent_propagation),
//       upper_bounds(Policy::UNSOLVED),
//       policy_cost_cache(std::vector<PolicyCost>(num_policies, Policy::UNKNOWN)),
//       cached_parents(std::vector<std::pair<State, int>>(num_policies)) { // State default constructor: StateID -1
// }

// BoundResult UpperBounds::run_plan(const TaskProxy &task_proxy, const State &state, std::vector<OperatorID> plan, int model) {
//     (void)task_proxy;
//     (void)state;
//     (void)plan;
//     (void)model;
//     return BoundResult {-1, Policy::UNKNOWN, Policy::UNKNOWN};
// }

// BoundResult UpperBounds::run_policy(const TaskProxy &task_proxy, const State &state, RemotePolicy &remote_policy, int model) {
//     if (policy_cost_cache[state][model] != Policy::UNKNOWN) {
//         // Policy ran before already; take upper bound immediately.
//         return BoundResult {model, policy_cost_cache[state][model], upper_bounds[state]};
//     }

//     std::vector<OperatorID> plan;
//     std::vector<State> path;
//     const auto [result_known, solved] = remote_policy.execute_get_plan_and_path(state, plan, path, model);
//     if (!result_known) {
//         // Did not finish execution; e.g. cutoff
//         return BoundResult {model, Policy::UNKNOWN, Policy::UNKNOWN};
//     }
//     if (!solved) {
//         for (State &state : path) {
//             policy_cost_cache[state][model] = Policy::UNSOLVED;
//         }
//         return BoundResult {model, Policy::UNSOLVED, upper_bounds[state]};
//     }

//     // Store path for later propagation, if enabled.
//     if (parent_propagation) {
//         register_path(task_proxy, plan, path, model);
//     }

//     int acc_cost = 0;
//     int curr_policy_cost = 0;
//     for (int i = path.size() - 1; i >= 0; --i) {
//         State curr_state = path[i];
//         assert(task_properties::is_goal_state(task_proxy, curr_state) || acc_cost != 0);
//         OperatorID curr_op = i > 0 ? plan[i - 1] : Policy::NO_OPERATOR;

//         if (policy_cost_cache[curr_state][model] == Policy::UNKNOWN) {
//             policy_cost_cache[curr_state][model] = curr_policy_cost;
//         }
//         if (!update_bound(curr_state, acc_cost)) {
//             acc_cost = upper_bounds[curr_state];
//         }
//         if (curr_op != Policy::NO_OPERATOR) {
//             acc_cost += task_proxy.get_operators()[curr_op.get_index()].get_cost();
//             curr_policy_cost += task_proxy.get_operators()[curr_op.get_index()].get_cost();
//         } else {
//             assert(curr_state == state);
//         }
//     }
//     if (parent_propagation) {
//         propagate_bounds();
//     }
//     return BoundResult {model, curr_policy_cost, upper_bounds[state]};
// }

// bool UpperBounds::inject_upper_bound(State &state, int new_bound) {
//     bool result = update_bound(state, new_bound);
//     // TODO: store as bound update (if applicable)
//     // TODO: initiate bound propagation
//     return result;
// }

// void UpperBounds::register_subsequent_bug_reporting(PolicyTestingBaseEngine *engine, const State &pool_state, int policy_value) {
//     subsequent_bug_reporting.insert({pool_state.get_id(), [engine, pool_state, policy_value](int new_value) {
//                                          if (engine->is_known_bug(pool_state)) {
//                                              return;
//                                          }
//                                          std::cout << "bug found in old pool state " << pool_state.get_id() << std::endl;
//                                          if (policy_value == Policy::UNSOLVED) {
//                                              engine->add_additional_bug(pool_state, TestResult(UNSOLVED_BUG_VALUE, new_value));
//                                              return;
//                                          }
//                                          engine->add_additional_bug(pool_state, TestResult {policy_value - new_value, new_value});
//                                      }
//                                     });
// }

// // PRIVATE //

// void UpperBounds::propagate_bounds(int model) {
//     if (!parent_propagation) {
//         return;
//     }

//     while (!improved_bounds.empty()) {
//         std::tuple<State, int, int> improved_bound = improved_bounds.back();
//         improved_bounds.pop_back();

//         State improved_state = std::get<0>(improved_bound);
//         int new_bound = std::get<2>(improved_bound);

//         std::vector<std::pair<State, int>> parents = cached_parents[improved_state];

//         // If model set (!= -1), just update parent at model idx, otherwise all parents.
//         unsigned long from = model == -1 ? 0 : model;
//         unsigned long to = model == -1 ? parents.size() - 1 : model;
//         for (unsigned long i = from; i <= to; ++i) {
//             auto [parent, cost] = parents[i];
//             if (!parent.get_registry()) {
//                 continue;
//             }

//             int old_parent_cost = upper_bounds[parent];
//             int new_parent_cost = new_bound + cost;
//             if (update_bound(parent, new_parent_cost)) {
//                 improved_bounds.emplace_back(parent, old_parent_cost, new_parent_cost);
//             }
//         }
//     }
// }

// bool UpperBounds::update_bound(State &state, PolicyCost new_cost) {
//     if (Policy::is_less(new_cost, upper_bounds[state])) {
//         if (parent_propagation) {
//             improved_bounds.emplace_back(state, upper_bounds[state], new_cost);
//         }
//         if (subsequent_bug_reporting.contains(state.get_id())) {
//             subsequent_bug_reporting.at(state.get_id())(new_cost);
//             subsequent_bug_reporting.erase(state.get_id());
//         }
//         upper_bounds[state] = new_cost;
//         return true;
//     }
//     return false;
// }

// void UpperBounds::register_path(const TaskProxy &task_proxy, std::vector<OperatorID> &plan, std::vector<State> &path, int model) {
//     assert(plan.size() == path.size() - 1);
//     for (int i = path.size() - 1; i > 0; --i) {
//         State curr_state = path[i];
//         State prev_state = path[i - 1];
//         OperatorID curr_op = plan[i - 1];

//         cached_parents[curr_state][model] = std::make_pair(prev_state, task_proxy.get_operators()[curr_op.get_index()].get_cost());
//     }
// }
// }
