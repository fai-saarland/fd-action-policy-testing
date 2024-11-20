#include "internal_planner_bias.h"

#include "../../plugins/plugin.h"

namespace policy_testing {
InternalPlannerBias::InternalPlannerBias(const plugins::Options &opts)
    : internal_planner(std::dynamic_pointer_cast<InternalPlannerPlanCostEstimator>(
                           opts.get<std::shared_ptr<PlanCostEstimator>>("internal_planner_oracle"))) {
    if (!internal_planner) {
        std::cerr << "You need to provide an InternalPlannerPlanCostEstimator" << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
    }
    register_sub_component(internal_planner.get());
}

void InternalPlannerBias::add_options_to_feature(plugins::Feature &feature) {
    feature.add_option<std::shared_ptr<PlanCostEstimator>>("internal_planner_oracle", "plan cost estimator (e.g. to compute h*)");
}

int InternalPlannerBias::bias(const State &state, unsigned int) {
    const int result = internal_planner->compute_trusted_value_with_cache(state);
    if (result == PlanCostEstimator::ReturnCode::DEAD_END) {
        return NEGATIVE_INFINITY;
    }
    return result;
}

bool InternalPlannerBias::can_exclude_state(const State &s) {
    return internal_planner->compute_trusted_value_with_cache(s) == PlanCostEstimator::ReturnCode::DEAD_END;
}

class InternalPlannerBiasFeature : public plugins::TypedFeature<FuzzingBias, InternalPlannerBias> {
public:
    InternalPlannerBiasFeature() : TypedFeature("internal_planner_bias") {
        InternalPlannerBias::add_options_to_feature(*this);
    }
};
static plugins::FeaturePlugin<InternalPlannerBiasFeature> _plugin;
} // namespace policy_testing
