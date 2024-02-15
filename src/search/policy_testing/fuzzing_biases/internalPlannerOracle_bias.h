#include "../engines/pool_fuzzer.h"

#include "../fuzzing_bias.h"
#include "../cost_estimators/internal_planner_cost_estimator.h"

namespace policy_testing {
class InternalPlannerOracleBias : public FuzzingBias {
public:
    explicit InternalPlannerOracleBias(const plugins::Options &opts);
    static void add_options_to_feature(plugins::Feature &feature);
    int bias(const State &state, unsigned int budget) override;
    bool can_exclude_state(const State &s) override;

private:
    std::shared_ptr<InternalPlannerPlanCostEstimator> internalPlannerOracle;
};
} //namespace policy_testing
