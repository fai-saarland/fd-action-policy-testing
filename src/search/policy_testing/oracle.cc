#include <queue>
#include "oracle.h"
#include "engines/testing_base_engine.h"
#include "cost_estimators/internal_planner_cost_estimator.h"
#include "../plugins/plugin.h"

namespace policy_testing {
Oracle::Oracle(const plugins::Options &opts) : TestingBaseComponent(opts),
                                               report_parent_bugs(opts.get<bool>("report_parent_bugs")),
                                               consider_intermediate_states(
                                                   opts.get<bool>("consider_intermediate_states")),
                                               enforce_intermediate(opts.get<bool>("enforce_intermediate")) {
}

void Oracle::set_engine(PolicyTestingBaseEngine *engine) {
    engine_ = engine;
}

int Oracle::get_optimal_cost(const State &s) const {
    static utils::HashMap<StateID, int> cache;
    const StateID state_id = s.get_id();
    auto it = cache.find(state_id);
    if (it != cache.end()) {
        return it->second;
    } else {
        InternalPlannerPlanCostEstimator optimal_planner(get_environment(), false);
        int cost = optimal_planner.compute_trusted_value(s);
        assert(cost != PlanCostEstimator::ReturnCode::UNKNOWN);
        int result = (cost == PlanCostEstimator::ReturnCode::DEAD_END) ? Policy::UNSOLVED : cost;
        cache.emplace(state_id, result);
        return result;
    }
}

bool
Oracle::confirm_bug(const State &state, BugValue bug_value) const {
    assert(bug_value != 0);
    assert(engine_);
    const int policy_cost = engine_->get_policy()->get_complete_policy_cost(state);
    assert(policy_cost == Policy::UNSOLVED || policy_cost >= 0);
    const int optimal_cost = get_optimal_cost(state);
    assert(optimal_cost == Policy::UNSOLVED || optimal_cost >= 0);
    assert((policy_cost == Policy::UNSOLVED && optimal_cost == Policy::UNSOLVED) ||
           (policy_cost == Policy::UNSOLVED && optimal_cost != Policy::UNSOLVED) ||
           (policy_cost != Policy::UNSOLVED && optimal_cost != Policy::UNSOLVED && optimal_cost <= policy_cost));
    if (bug_value == UNSOLVED_BUG_VALUE) {
        return policy_cost == Policy::UNSOLVED && optimal_cost != Policy::UNSOLVED;
    } else {
        return (policy_cost == Policy::UNSOLVED && optimal_cost != Policy::UNSOLVED) ||
               (optimal_cost + bug_value <= policy_cost);
    }
}

void Oracle::report_parents_as_bugs(Policy &policy, const State &s, TestResult test_result) {
    if (test_result.bug_value <= 0) {
        return;
    }
    if (test_result.upper_cost_bound == Policy::UNSOLVED) {
        // cannot update cost bounds
        std::queue<StateID> queue;
        queue.push(s.get_id());
        utils::HashSet<StateID> processed;
        while (!queue.empty()) {
            StateID current_state = queue.front();
            queue.pop();
            if (!processed.insert(current_state).second) {
                continue;
            }
            for (const StateID &parent : policy.get_policy_parent_states(current_state)) {
                State parent_state = get_state_registry().lookup_state(parent);
                const BugValue old_parent_bug_value = engine_->get_stored_bug_result(parent_state).bug_value;
                if (test_result.bug_value <= old_parent_bug_value) {
                    continue;
                }
                engine_->add_additional_bug(parent_state, test_result);
                queue.push(parent);
            }
        }
    } else {
        // update parent cost bounds
        std::queue<std::pair<StateID, PolicyCost>> queue; // pairs of state and upper cost bounds
        queue.emplace(s.get_id(), test_result.upper_cost_bound);
        utils::HashSet<StateID> processed;
        while (!queue.empty()) {
            auto [current_state, current_cost_bound] = queue.front();
            queue.pop();
            if (!processed.insert(current_state).second) {
                continue;
            }
            for (const StateID &parent : policy.get_policy_parent_states(current_state)) {
                State parent_state = get_state_registry().lookup_state(parent);
                const BugValue old_parent_bug_value = engine_->get_stored_bug_result(parent_state).bug_value;
                if (test_result.bug_value <= old_parent_bug_value) {
                    continue;
                }
                PolicyCost parent_cost_bound = current_cost_bound + policy.read_action_cost(parent_state);
                engine_->add_additional_bug(parent_state, TestResult(test_result.bug_value, parent_cost_bound));
                queue.emplace(parent, parent_cost_bound);
            }
        }
    }
}

TestResult
Oracle::test_driver(Policy &policy, const PoolEntry &entry) {
    const State &pool_state = entry.state;
    if (engine_->is_known_bug(pool_state) && !enforce_intermediate) {
        return engine_->get_stored_bug_result(pool_state);
    }
    if (consider_intermediate_states || enforce_intermediate) {
        std::vector<State> path = policy.execute_get_path_fragment(pool_state);
        assert(!path.empty());
        // call test for intermediate states (in reverse order)
        for (auto it = path.crbegin(); it != std::prev(path.crend()); ++it) {
            const State &intermediate_state = *it;
            if (policy.is_goal(intermediate_state) || engine_->is_known_bug(intermediate_state)) {
                continue;
            }
            const TestResult intermediate_test_result = test(policy, intermediate_state);
            if (intermediate_test_result.bug_value > 0) {
                engine_->add_additional_bug(intermediate_state, intermediate_test_result);
                if (report_parent_bugs) {
                    report_parents_as_bugs(policy, intermediate_state, intermediate_test_result);
                    return intermediate_test_result;
                }
            }
        }

        if (engine_->is_known_bug(pool_state)) {
            return engine_->get_stored_bug_result(pool_state);
        }
    }

    // main test
    const TestResult test_result = test(policy, pool_state);
    if (test_result.bug_value > 0 && report_parent_bugs) {
        report_parents_as_bugs(policy, pool_state, test_result);
    }
    return test_result;
}

void
Oracle::add_options_to_feature(plugins::Feature &feature) {
    TestingBaseComponent::add_options_to_feature(feature);
    feature.add_option<bool>("report_parent_bugs",
                             "For every reported bug go through all policy parents and report them as bugs as well.",
                             "false");
    feature.add_option<bool>("consider_intermediate_states", "Run bug test also on intermediate states.", "false");
    feature.add_option<bool>("enforce_intermediate",
                             "Consider intermediate states even if bug candidate is known to be a bug", "false");
}

static class OraclePlugin : public plugins::TypedCategoryPlugin<Oracle> {
public:
    OraclePlugin() : TypedCategoryPlugin("Oracle") {
        document_synopsis(
            "This page describes the different Oracles."
            );
    }
}
_category_plugin;
} // namespace policy_testing
