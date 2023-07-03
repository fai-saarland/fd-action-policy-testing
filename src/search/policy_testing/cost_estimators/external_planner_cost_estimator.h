#pragma once

#include "../cost_estimator.h"
#include "../plan_file_parser.h"

#include <string>

namespace options {
class Options;
class OptionParser;
} // namespace options

namespace policy_testing {
class ExternalPlannerPlanCostEstimator : public PlanCostEstimator {
public:
    explicit ExternalPlannerPlanCostEstimator(const options::Options &opts);
    static void add_options_to_parser(options::OptionParser &parser);

    int compute_value(const State &state) override;

    /**
     * Call external planner to compute plan. Returns 0 if plan was found,
     * DEAD_END if external planner proved state to be a dead end, UNKNOWN
     * otherwise.
     **/
    int run_planner(const State &s, std::vector<OperatorID> &plan);

protected:
    void initialize() override;

private:
    static void cleanup();
    bool load(const std::string &file_name, std::vector<OperatorID> &plan);

    std::unique_ptr<PlanFileParser> plan_file_parser_;

    std::string downward_path_;
    std::string preprocess_path_;
    std::string params_;

    // const int plan_found_exit_code_;
    const int unsolvable_exit_code_;
};
} // namespace policy_testing
