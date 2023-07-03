#pragma once

#include "../../search_engine.h"
#include "../pool.h"
#include "../testing_environment.h"
#include "../utils.h"
#include "testing_base_engine.h"

#include <memory>
#include <string>

// pool oracle evaluator might be broken after having changed InternalPlannerPlanCostEstimator::compute_value

class Evaluator;

namespace options {
class Options;
class OptionParser;
} // namespace options

namespace policy_testing {
class PlanCostEstimator;

class PoolOracleEvaluatorEngine : public PolicyTestingBaseEngine {
public:
    explicit PoolOracleEvaluatorEngine(const options::Options &opts);
    static void add_options_to_parser(options::OptionParser &parser);

    void print_statistics() const override;
    void print_timed_statistics() const override { }

protected:
    SearchStatus step() override;

private:
    TestingEnvironment env_;
    Pool pool_;

    std::shared_ptr<PlanCostEstimator> cost_estimator_;
    std::shared_ptr<Evaluator> eval_;

    const int step_time_limit_;
    const unsigned max_steps_;
    const unsigned first_step_;
    const unsigned end_step_;
    const bool debug_;

    unsigned step_ = 0;
    unsigned solved_ = 0;
    unsigned dead_ends_ = 0;
    unsigned unknown_ = 0;
};
} // namespace policy_testing
