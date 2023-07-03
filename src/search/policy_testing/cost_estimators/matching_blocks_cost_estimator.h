#pragma once

#include "../cost_estimator.h"

#include <vector>

namespace options {
class Options;
class OptionParser;
} // namespace options

namespace policy_testing {
class MatchingBlocksQualPlanCostEstimator : public PlanCostEstimator {
public:
    explicit MatchingBlocksQualPlanCostEstimator(const options::Options &opts);
    static void add_options_to_parser(options::OptionParser &parser);

    int compute_value(const State &state) override;

protected:
    void initialize() override;

private:
    std::vector<std::pair<int, int>> block_solid_;
    std::vector<std::pair<int, int>> wrong_hand_;
    std::vector<unsigned> must_remain_solid_;
};
} // namespace policy_testing
