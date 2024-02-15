#include "bounded_lookahead_oracle.h"

#include "../../plugins/plugin.h"
#include "../../task_utils/task_properties.h"
#include "../out_of_resource_exception.h"

#include <cassert>
#include <utility>
#include <vector>

namespace policy_testing {
BoundedLookaheadOracle::BoundedLookaheadOracle(const plugins::Options &opts)
    : Oracle(opts),
      depth_(opts.get<int>("depth")),
      max_evaluation_steps(opts.get<int>("max_evaluation_steps")),
      dead_end_eval(opts.contains("dead_end_eval") ?
                    opts.get<std::shared_ptr<Evaluator>>("dead_end_eval"): nullptr),
      cache_results(opts.get<bool>("cache_results")) {
}

void
BoundedLookaheadOracle::add_options_to_feature(plugins::Feature &feature) {
    Oracle::add_options_to_feature(feature);
    feature.add_option<int>("depth", "", "2");
    feature.add_option<int>("max_evaluation_steps",
                            "Maximal number of steps in evaluation of policy in unrelaxed state.", "-1");
    feature.add_option<std::shared_ptr<Evaluator>>("dead_end_eval",
                                                   "Evaluator used for dead end detection in policy evaluation of dead end states.",
                                                   plugins::ArgumentInfo::NO_DEFAULT);
    feature.add_option<bool>("cache_results", "Cache the results of oracle invocations", "true");
}

TestResult
BoundedLookaheadOracle::test(Policy &policy, const State &state) {
    if (cache_results) {
        auto it = result_cache.find(state.get_id());
        if (it != result_cache.end()) {
            return it->second;
        }
    }

    const PolicyCost lower_policy_cost_bound = policy.compute_lower_policy_cost_bound(state).first;

    struct Node {
        State state;
        PolicyCost g_value;

        Node(State state, PolicyCost g_value) : state(std::move(state)), g_value(g_value) {}
    };

    // TODO this arrangement with open and closed lists is certainly not optimal and could be improved
    // such that the depth is only considered for the cutoff. Also tests should only be conducted if this makes sense
    // considering the g_value (perhaps maintain the minimal g_value for each seen state)
    // however the overhead for testing a state twice is negligible (since the policy is cached)
    // for non-unit cost domains the oracle should be sound but maintaining a closed list for each depth is suboptimal
    // as depth is not equal to g_value anymore and there could be a new state with a lower g_value that is pruned

    std::vector<std::vector<Node>> open(depth_);
    std::vector<utils::HashSet<StateID>> closed(depth_);
    std::vector<OperatorID> aops;

    open[0].emplace_back(state, 0);
    int depth = 0;
    while (depth >= 0) {
        if (open[depth].empty()) {
            --depth;
            continue;
        }
        const Node current = open[depth].back();
        const State &current_state = current.state;
        const PolicyCost g_value = current.g_value;
        open[depth].pop_back();
        if (!closed[depth].insert(current_state.get_id()).second) {
            continue;
        }
        if (task_properties::is_goal_state(get_task_proxy(), current_state)) {
            if (lower_policy_cost_bound == Policy::UNSOLVED) {
#ifndef NDEBUG
                if (debug_) {
                    assert(confirm_bug(state, UNSOLVED_BUG_VALUE));
                }
#endif
                if (cache_results) {
                    result_cache[state.get_id()] = TestResult(UNSOLVED_BUG_VALUE, g_value);
                }
                return TestResult(UNSOLVED_BUG_VALUE, g_value);
            } else if (lower_policy_cost_bound > g_value) {
#ifndef NDEBUG
                if (debug_) {
                    assert(confirm_bug(state, lower_policy_cost_bound - g_value));
                }
#endif
                if (cache_results) {
                    result_cache[state.get_id()] = TestResult(lower_policy_cost_bound - g_value, g_value);
                }
                return TestResult(lower_policy_cost_bound - g_value, g_value);
            }
            continue;
        }
        generate_applicable_ops(current_state, aops);
        if (depth + 1 == depth_) {
            for (const auto &op : aops) {
                State succ = get_successor_state(current_state, op);
                PolicyCost succ_g_value = policy.get_operator_cost(op) + g_value;

                int succ_cost_limit = -1;
                bool evaluate_succ = true;
                if (lower_policy_cost_bound != Policy::UNSOLVED) {
                    // bug criterion: plan_cost > succ_plan_cost + succ_g_value
                    succ_cost_limit = lower_policy_cost_bound - succ_g_value;
                    evaluate_succ = succ_cost_limit >= 0;
                }

                const int succ_plan_cost = evaluate_succ ?
                    policy.lazy_compute_policy_cost(succ, succ_cost_limit, max_evaluation_steps, dead_end_eval)
                        : Policy::UNSOLVED;

                if (succ_plan_cost != Policy::UNSOLVED) {
                    if (lower_policy_cost_bound == Policy::UNSOLVED) {
#ifndef NDEBUG
                        if (debug_) {
                            assert(confirm_bug(state, UNSOLVED_BUG_VALUE));
                        }
#endif
                        if (cache_results) {
                            result_cache[state.get_id()] =
                                TestResult(UNSOLVED_BUG_VALUE, succ_plan_cost + succ_g_value);
                        }
                        return TestResult(UNSOLVED_BUG_VALUE, succ_plan_cost + succ_g_value);
                    } else if (lower_policy_cost_bound > succ_plan_cost + succ_g_value) {
#ifndef NDEBUG
                        if (debug_) {
                            assert(confirm_bug(state, lower_policy_cost_bound - succ_plan_cost - succ_g_value));
                        }
#endif
                        if (cache_results) {
                            result_cache[state.get_id()] = TestResult(
                                lower_policy_cost_bound - succ_plan_cost - succ_g_value,
                                succ_plan_cost + succ_g_value);
                        }
                        return TestResult(lower_policy_cost_bound - succ_plan_cost - succ_g_value,
                                          succ_plan_cost + succ_g_value);
                    }
                }
                if (are_limits_reached()) {
                    throw OutOfResourceException();
                }
            }
        } else {
            assert(open[depth + 1].empty());
            ++depth;
            for (const auto &op : aops) {
                State succ = get_successor_state(current_state, op);
                PolicyCost op_cost = policy.get_operator_cost(op);
                open[depth].emplace_back(succ, g_value + op_cost);
            }
        }
        aops.clear();
    }
    if (cache_results) {
        result_cache[state.get_id()] = {};
    }
    return {};
}

class BoundedLookaheadOracleFeature : public plugins::TypedFeature<Oracle, BoundedLookaheadOracle> {
public:
    BoundedLookaheadOracleFeature() : TypedFeature("bounded_lookahead_oracle") {
        BoundedLookaheadOracle::add_options_to_feature(*this);
    }
};
static plugins::FeaturePlugin<BoundedLookaheadOracleFeature> _plugin;
} // namespace policy_testing
