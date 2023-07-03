#pragma once

#include "../oracle.h"

namespace options {
class Options;
class OptionParser;
} // namespace options

namespace policy_testing {
class BoundedLookaheadOracle : public Oracle {
public:
    explicit BoundedLookaheadOracle(const options::Options &opts);
    static void add_options_to_parser(options::OptionParser &parser);

    TestResult test(Policy &policy, const State &state) override;

private:
    const int depth_;

    // maximal number of steps in evaluation of policy in unrelaxed state
    int max_evaluation_steps;
    // evaluator used for dead end detection in policy evaluation of dead end states
    std::shared_ptr<Evaluator> dead_end_eval;

    const bool cache_results;
    utils::HashMap<StateID, TestResult> result_cache;
};
} // namespace policy_testing
