#include "../engines/pool_fuzzer.h"

#include "../fuzzing_bias.h"
#include "../../option_parser.h"
#include "../cost_estimators/internal_planner_cost_estimator.h"

namespace policy_testing {
class InternalPlannerOracleBias : public FuzzingBias {
public:
    explicit InternalPlannerOracleBias(const options::Options &opts);
    static void add_options_to_parser(options::OptionParser &parser);
    int bias(const State &state, unsigned int budget) override;
    bool can_exclude_state(const State &s) override;

private:
    std::shared_ptr<InternalPlannerPlanCostEstimator> internalPlannerOracle;
};
} //namespace policy_testing
