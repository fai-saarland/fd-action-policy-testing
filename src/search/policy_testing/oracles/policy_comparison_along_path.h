#pragma once

#include "../oracle.h"

namespace policy_testing {
class PolicyComparisonAlongPath : public Oracle {
public:
    explicit PolicyComparisonAlongPath(const plugins::Options &opts);
    static void add_options_to_feature(plugins::Feature &feature);

    TestResult test(Policy &policy, const State &state) override;

private:
    int num_per_state;
    double run_prob;

    utils::RandomNumberGenerator rng;
};
} // namespace policy_testing
