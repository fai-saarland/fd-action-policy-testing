#include "oracle.h"
#include "engines/testing_base_engine.h"

#include "cost_estimators/internal_planner_cost_estimator.h"

#include "../plugin.h"

namespace policy_testing {
Oracle::Oracle(const options::Options &opts) : TestingBaseComponent(opts),
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

void Oracle::report_parents_as_bugs(Policy &policy, const State &s, BugValue bug_value) {
    if (bug_value <= 0) {
        return;
    }
    std::queue<StateID> queue;
    queue.push(s.get_id());
    std::unordered_set<int> processed;
    while (!queue.empty()) {
        StateID current_state = queue.front();
        queue.pop();
        if (!processed.insert(current_state.get_value()).second) {
            continue;
        }
        for (StateID parent : policy.get_policy_parent_states(current_state)) {
            State parent_state = get_state_registry().lookup_state(parent);
            const BugValue old_parent_bug_value = engine_->get_stored_bug_value(parent_state);
            if (bug_value <= old_parent_bug_value) {
                continue;
            }
            engine_->add_additional_bug(parent_state, bug_value);
            queue.push(parent);
        }
    }
}

BugValue
Oracle::test_driver(Policy &policy, const PoolEntry &entry) {
    const State &pool_state = entry.state;
    if (engine_->is_known_bug(pool_state) && !enforce_intermediate) {
        return engine_->get_stored_bug_value(pool_state);
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
            const BugValue intermediate_bug_value = test(policy, intermediate_state).bug_value;
            if (intermediate_bug_value > 0) {
                engine_->add_additional_bug(intermediate_state, intermediate_bug_value);
                if (report_parent_bugs) {
                    report_parents_as_bugs(policy, intermediate_state, intermediate_bug_value);
                    return intermediate_bug_value;
                }
            }
        }

        if (engine_->is_known_bug(pool_state)) {
            return engine_->get_stored_bug_value(pool_state);
        }
    }

    // main test
    BugValue bug_value = test(policy, pool_state).bug_value;
    if (bug_value > 0 && report_parent_bugs) {
        report_parents_as_bugs(policy, pool_state, bug_value);
    }
    return bug_value;
}

void
Oracle::add_options_to_parser(options::OptionParser &parser) {
    TestingBaseComponent::add_options_to_parser(parser);
    parser.add_option<bool>("report_parent_bugs",
                            "For every reported bug go through all policy parents and report them as bugs as well.",
                            "false");
    parser.add_option<bool>("consider_intermediate_states", "Run bug test also on intermediate states.", "false");
    parser.add_option<bool>("enforce_intermediate",
                            "Consider intermediate states even if bug candidate is known to be a bug", "false");
}

PluginTypePlugin<Oracle> _plugin_type("Oracle", "");
} // namespace policy_testing
