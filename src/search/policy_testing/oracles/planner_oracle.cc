#include "planner_oracle.h"

#include "../../plugins/plugin.h"

namespace policy_testing {
PlannerOracle::PlannerOracle(const plugins::Options &opts)
    : Oracle(opts),
      estimator(opts.get<std::shared_ptr<PlanCostEstimator>>("oracle")),
      cache_results(opts.get<bool>("cache_results")) {
    register_sub_component(estimator.get());
}

void PlannerOracle::add_options_to_feature(plugins::Feature &feature) {
    Oracle::add_options_to_feature(feature);
    feature.add_option<std::shared_ptr<PlanCostEstimator>>("oracle", "Plan cost estimator.");
    feature.add_option<bool>("cache_results", "Cache the results of oracle invocations", "true");
}

TestResult PlannerOracle::test(Policy &policy, const State &state) {
    if (cache_results) {
        auto it = result_cache.find(state.get_id());
        if (it != result_cache.end()) {
            return it->second;
        }
    }
    const PolicyCost lower_policy_cost_bound = policy.compute_lower_policy_cost_bound(state).first;
    const int oracle_cost = estimator->compute_value(state);
    if (oracle_cost == PlanCostEstimator::UNKNOWN || oracle_cost == PlanCostEstimator::DEAD_END) {
        if (cache_results) {
            result_cache[state.get_id()] = {};
        }
        return {};
    }
    if (lower_policy_cost_bound == Policy::UNSOLVED) {
        if (cache_results) {
            result_cache[state.get_id()] = TestResult(UNSOLVED_BUG_VALUE, oracle_cost);
        }
        return TestResult(UNSOLVED_BUG_VALUE, oracle_cost);
    }
    if (oracle_cost < lower_policy_cost_bound) {
        if (cache_results) {
            result_cache[state.get_id()] = TestResult(lower_policy_cost_bound - oracle_cost, oracle_cost);
        }
        return TestResult(lower_policy_cost_bound - oracle_cost, oracle_cost);
    }
    if (cache_results) {
        result_cache[state.get_id()] = {};
    }
    return {};
}

class PlannerOracleFeature
    : public plugins::TypedFeature<Oracle, PlannerOracle> {
public:
    PlannerOracleFeature() : TypedFeature("planner_oracle") {
        PlannerOracle::add_options_to_feature(*this);
    }
};
static plugins::FeaturePlugin<PlannerOracleFeature> _plugin;
} // namespace policy_testing
