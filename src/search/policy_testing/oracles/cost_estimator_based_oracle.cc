#include "cost_estimator_based_oracle.h"

#include "../../option_parser.h"
#include "../../plugin.h"

namespace policy_testing {
EstimatorBasedOracle::EstimatorBasedOracle(const options::Options &opts)
    : Oracle(opts),
      estimator_(opts.get<std::shared_ptr<PlanCostEstimator>>("oracle")),
      cache_results(opts.get<bool>("cache_results")) {
    register_sub_component(estimator_.get());
}

void
EstimatorBasedOracle::add_options_to_parser(options::OptionParser &parser) {
    Oracle::add_options_to_parser(parser);
    parser.add_option<std::shared_ptr<PlanCostEstimator>>("oracle");
    parser.add_option<bool>("cache_results", "Cache the results of oracle invocations", "true");
}

TestResult
EstimatorBasedOracle::test(Policy &policy, const State &state) {
    if (cache_results) {
        auto it = result_cache.find(state.get_id());
        if (it != result_cache.end()) {
            return it->second;
        }
    }
    const PolicyCost lower_policy_cost_bound = policy.compute_lower_policy_cost_bound(state).first;
    const int oracle_cost = estimator_->compute_value(state);
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

static Plugin<Oracle> _plugin(
    "estimator_based_oracle",
    options::parse<Oracle, EstimatorBasedOracle>);
} // namespace policy_testing
