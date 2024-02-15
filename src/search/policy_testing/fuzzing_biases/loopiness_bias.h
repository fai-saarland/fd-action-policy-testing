#include "../fuzzing_bias.h"
#include "../policy.h"
#include "../../plan_manager.h"
#include "../../heuristics/relaxation_heuristic.h"
#include "../cost_estimators/internal_planner_cost_estimator.h"


namespace policy_testing {
class LoopinessBias : public PolicyBasedBias {
public:
    explicit LoopinessBias(const plugins::Options &opts);
    static void add_options_to_feature(plugins::Feature &feature);
    int bias_without_maximization(const State &state, unsigned int budget);
    int bias_with_maximization(const State &state, unsigned int budget);
    int bias(const State &state, unsigned int budget) override;
    bool can_exclude_state(const State &s) override;

protected:
    void initialize() override;

private:
    std::shared_ptr<relaxation_heuristic::RelaxationHeuristic> h;
    const std::shared_ptr<InternalPlannerPlanCostEstimator> internalPlanCostEstimator;
    bool omit_maximization;
    const bool omit_maximization_if_task_invertible;
};
} //namespace policy_testing
