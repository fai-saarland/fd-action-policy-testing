#include "unrelaxation_oracle.h"

#include <memory>

#include "../simulations/merge_and_shrink/abstraction_builder.h"

namespace policy_testing {
UnrelaxationOracle::UnrelaxationOracle(const plugins::Options &opts)
    : NumericDominanceOracle(opts), operations_per_state(static_cast<unsigned int>(std::max(opts.get<int>("operations_per_state"), 1))),
      max_evaluation_steps(opts.get<int>("max_evaluation_steps")),
      dead_end_eval(opts.contains("dead_end_eval") ?
                    opts.get<std::shared_ptr<Evaluator>>("dead_end_eval"): nullptr) {
}

void
UnrelaxationOracle::initialize() {
    NumericDominanceOracle::initialize();
}

void
UnrelaxationOracle::add_options_to_feature(plugins::Feature &feature) {
    NumericDominanceOracle::add_options_to_feature(feature);
    feature.add_option<int>("operations_per_state", "Number of unrelaxations to check in each state. Values smaller than 1 will be set to 1.", "4");
    feature.add_option<int>("max_evaluation_steps",
                            "Maximal number of steps in evaluation of policy in unrelaxed state.", "-1");
    feature.add_option<std::shared_ptr<Evaluator>>("dead_end_eval",
                                                   "Evaluator used for dead end detection in policy evaluation of dead end states.",
                                                   plugins::ArgumentInfo::NO_DEFAULT);
}

std::vector<std::pair<State, NumericDominanceOracle::DominanceValue>>
UnrelaxationOracle::unrelax(const State &s) const {
    // dominance_value is D(unrelaxed_state, relaxed_state)
    std::vector<std::pair<State, NumericDominanceOracle::DominanceValue>> result;
    std::vector<int> relaxed_state = s.get_values();

    // compute all possible unrelaxed states obtainable by changing one variable along and the dominance values
    const unsigned int num_variables = simulations::global_simulation_task()->get_num_variables();
    for (int var = 0; var < num_variables; ++var) {
        const unsigned int domain_size = simulations::global_simulation_task()->get_variable_domain_size(var);
        const int relaxed_value = relaxed_state[var];
        for (int unrelaxed_value = 0; unrelaxed_value < domain_size; ++unrelaxed_value) {
            if (unrelaxed_value == relaxed_value) {
                continue;
            }
            std::vector<int> unrelaxed_state = relaxed_state;
            unrelaxed_state[var] = unrelaxed_value;
            const int dominance_value = D(unrelaxed_state, relaxed_state);
            if (dominance_value == simulations::MINUS_INFINITY) {
                continue;
            }
            result.emplace_back(get_state_registry().insert_state(unrelaxed_state), dominance_value);
        }
    }

    simulations::simulations_rng.shuffle(result);
    result.resize(std::min<unsigned int>(result.size(), operations_per_state));

    if (debug_) {
        std::cout << "(Debug) Constructed  " << result.size() << " unrelaxed states:" << std::endl;
        for (const auto &[state, dominance_value] : result) {
            std::cout << "(Debug) " << state << " (dominance value: " << dominance_value << ")" << std::endl;
        }
    }
    return result;
}

TestResult
UnrelaxationOracle::test(Policy &policy, const State &relaxed_state) {
    const auto [lower_cost_bound_relaxed,
                policy_bound_is_exact] = policy.compute_lower_policy_cost_bound(relaxed_state);

    BugValue bug_value = local_bug_test(policy, relaxed_state);

    // skip test if bug could already be confirmed by local test
    if (bug_value > 0) {
#ifndef NDEBUG
        if (debug_) {
            assert(confirm_bug(relaxed_state, bug_value));
        }
#endif
        if (bug_value < UNSOLVED_BUG_VALUE && policy_bound_is_exact) {
            assert(lower_cost_bound_relaxed == policy.get_complete_policy_cost(relaxed_state));
            return TestResult(bug_value, lower_cost_bound_relaxed - bug_value);
        } else {
            return TestResult(bug_value, Policy::UNSOLVED);
        }
    }

    PolicyCost upper_cost_bound = Policy::UNSOLVED;

    for (const auto &[unrelaxed_state, dominance_value] : unrelax(relaxed_state)) {
        if (dominance_value == simulations::MINUS_INFINITY) {
            continue;
        }
        assert(relaxed_state != unrelaxed_state);
#ifndef NDEBUG
        if (debug_) {
            assert(confirm_dominance_value(unrelaxed_state, relaxed_state, dominance_value));
        }
#endif
        int unrelaxed_cost_limit = -1;
        if (lower_cost_bound_relaxed != Policy::UNSOLVED) {
            // bug criterion: cost_unrelaxed < cost_relaxed + dominance_value
            unrelaxed_cost_limit = lower_cost_bound_relaxed + dominance_value;
            if (unrelaxed_cost_limit < 0) {
                // test could not possibly identify a bug since the unrelaxed cost cannot be negative
                continue;
            }
        }

        const int cost_unrelaxed = policy.lazy_compute_policy_cost(
            unrelaxed_state, unrelaxed_cost_limit, max_evaluation_steps, dead_end_eval);

        if (lower_cost_bound_relaxed == Policy::UNSOLVED) {
            // policy does not solve relaxed state -> if it solves the unrelaxed state, we have a bug
            if (cost_unrelaxed != Policy::UNSOLVED) {
                bug_value = UNSOLVED_BUG_VALUE;
                upper_cost_bound = cost_unrelaxed - dominance_value;
                // bug value is maximal, no need to try to increase it
                break;
            }
        } else {
            assert(lower_cost_bound_relaxed >= 0);
            if (cost_unrelaxed == Policy::UNSOLVED) {
                continue;
            }
            assert(cost_unrelaxed >= 0);
            assert(dominance_value > simulations::MINUS_INFINITY);
            assert(bug_value != UNSOLVED_BUG_VALUE);
            assert(bug_value >= 0);
            if ((cost_unrelaxed - lower_cost_bound_relaxed) < dominance_value) {
                bug_value = std::max(bug_value, dominance_value - (cost_unrelaxed - lower_cost_bound_relaxed));
                if (bug_value > 0) {
                    upper_cost_bound = cost_unrelaxed - dominance_value;
                    // do not try to increase bug values because it is too expensive
                    break;
                }
            }
        }
    }
#ifndef NDEBUG
    if (debug_) {
        assert(bug_value == 0 || confirm_bug(relaxed_state, bug_value));
    }
#endif
    return TestResult(bug_value, upper_cost_bound);
}

class UnrelaxationOracleFeature : public plugins::TypedFeature<Oracle, UnrelaxationOracle> {
public:
    UnrelaxationOracleFeature() : TypedFeature("unrelaxation_oracle") {
        UnrelaxationOracle::add_options_to_feature(*this);
    }
};
static plugins::FeaturePlugin<UnrelaxationOracleFeature> _plugin;
} // namespace policy_testing
