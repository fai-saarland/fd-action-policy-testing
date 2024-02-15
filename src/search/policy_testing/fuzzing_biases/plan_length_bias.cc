#include "plan_length_bias.h"

#include "../../plugins/plugin.h"

namespace policy_testing {
PlanLengthBias::PlanLengthBias(const plugins::Options &opts)
    : PolicyBasedBias(opts) {
}

void
PlanLengthBias::add_options_to_feature(plugins::Feature &feature) {
    PolicyBasedBias::add_options_to_feature(feature);
}

int
PlanLengthBias::bias(const State &state, unsigned int budget) {
    const unsigned int step_limit = get_step_limit(budget);
    const PolicyCost cost = policy->compute_policy_cost(state, step_limit, false);
    if (cost == Policy::UNSOLVED) {
        return POSITIVE_INFINITY;
    } else if (cost == Policy::UNKNOWN) {
        return step_limit + 1;
    } else {
        return cost;
    }
}

class PlanLengthBiasFeature : public plugins::TypedFeature<FuzzingBias, PlanLengthBias> {
public:
    PlanLengthBiasFeature() : TypedFeature("plan_length_bias") {
        PlanLengthBias::add_options_to_feature(*this);
    }
};
static plugins::FeaturePlugin<PlanLengthBiasFeature> _plugin;
} // namespace policy_testing
