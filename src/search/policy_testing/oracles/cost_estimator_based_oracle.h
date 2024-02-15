#pragma once

#include "../cost_estimator.h"
#include "../oracle.h"

namespace policy_testing {
class EstimatorBasedOracle : public Oracle {
public:
    explicit EstimatorBasedOracle(const plugins::Options &opts);
    static void add_options_to_feature(plugins::Feature &feature);

    TestResult test(Policy &policy, const State &state) override;

private:
    std::shared_ptr<PlanCostEstimator> estimator_;
    const bool cache_results;
    utils::HashMap<StateID, TestResult> result_cache;
};
} // namespace policy_testing
