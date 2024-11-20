#include "../engines/pool_fuzzer.h"

#include "../fuzzing_bias.h"
#include "../cost_estimators/internal_planner_cost_estimator.h"

namespace policy_testing {
class InternalPlannerBias : public FuzzingBias {
public:
    explicit InternalPlannerBias(const plugins::Options &opts);
    static void add_options_to_feature(plugins::Feature &feature);
    int bias(const State &state, unsigned int budget) override;
    bool can_exclude_state(const State &s) override;

private:
    std::shared_ptr<InternalPlannerPlanCostEstimator> internal_planner;
};
} //namespace policy_testing
