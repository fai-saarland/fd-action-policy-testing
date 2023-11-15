#include "../fuzzing_bias.h"
#include "../../option_parser.h"
#include "../../plan_manager.h"
#include "../../evaluator.h"
#include "../policy.h"
#include "../oracle.h"
#include "../cost_estimators/internal_planner_cost_estimator.h"

namespace policy_testing {
class SurfaceBias : public PolicyBasedBias {
public:
    explicit SurfaceBias(const options::Options &opts);
    static void add_options_to_parser(options::OptionParser &parser);
    int bias_without_maximization(const State &state, unsigned int budget);
    int bias_with_maximization(const State &state, unsigned int budget);
    int bias(const State &state, unsigned int budget) override;
    bool can_exclude_state(const State &s) override;

protected:
    const std::shared_ptr<Evaluator> h;
    const std::shared_ptr<InternalPlannerPlanCostEstimator> internalPlanCostEstimator;
    const bool omit_maximization;
};
} //namespace policy_testing
