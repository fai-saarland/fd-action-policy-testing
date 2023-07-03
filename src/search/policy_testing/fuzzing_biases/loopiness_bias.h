#include "../fuzzing_bias.h"
#include "../policy.h"
#include "../../plan_manager.h"
#include "../../heuristics/relaxation_heuristic.h"
#include "../cost_estimators/internal_planner_cost_estimator.h"


namespace policy_testing {
class loopinessBias : public PolicyBasedBias {
public:
    explicit loopinessBias(const options::Options &opts);
    static void add_options_to_parser(options::OptionParser &parser);
    int bias(const State &state, unsigned int budget) override;
    bool can_exclude_state(const State &s) override;

private:
    std::shared_ptr<relaxation_heuristic::RelaxationHeuristic> h;
    const std::shared_ptr<InternalPlannerPlanCostEstimator> internalPlanCostEstimator;
};
} //namespace policy_testing
