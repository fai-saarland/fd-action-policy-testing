#pragma once

#include "../oracle.h"
#include "../../utils/rng.h"
#include <queue>
#include <map>

namespace policy_testing {
class PolicyComparisonOracleOnline : public Oracle {
public:
    explicit PolicyComparisonOracleOnline(const plugins::Options &opts);
    static void add_options_to_feature(plugins::Feature &feature);

    TestResult test(Policy &policy, const State &state) override;

private:
    // First: num times policy was run; Second: num bugs found through policy
    std::vector<std::pair<int, int>> bugs_found_by_policy;

    int num_port_policies = 0;
    std::map<int, double> most_found; // (port_num, ratio)
    utils::RandomNumberGenerator rng;

    void update_ratios(int port_model);
};
} // namespace policy_testing
