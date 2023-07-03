#pragma once

#include "../../heuristics/max_heuristic.h"
#include "../cost_estimator.h"

#include <vector>

namespace options {
class Options;
class OptionParser;
} // namespace options

namespace policy_testing {
class SpannerQualPlanCostEstimator : public PlanCostEstimator, max_heuristic::HSPMaxHeuristic {
public:
    explicit SpannerQualPlanCostEstimator(const options::Options &opts);
    static void add_options_to_parser(options::OptionParser &parser);

    int compute_value(const State &state) override;

protected:
    void initialize() override;

private:
    std::vector<std::pair<
                    relaxation_heuristic::Proposition *,
                    relaxation_heuristic::Proposition *>>
    spanners_;
};
} // namespace policy_testing
