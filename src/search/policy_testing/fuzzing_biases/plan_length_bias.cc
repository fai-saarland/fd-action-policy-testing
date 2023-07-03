#include "plan_length_bias.h"

#include "../../option_parser.h"
#include "../../plugin.h"

namespace policy_testing {
PlanLengthBias::PlanLengthBias(const options::Options &opts)
    : PolicyBasedBias(opts) {
}

void
PlanLengthBias::add_options_to_parser(options::OptionParser &parser) {
    PolicyBasedBias::add_options_to_parser(parser);
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

static Plugin<FuzzingBias> _plugin_bias_planlen(
    "plan_length_bias",
    options::parse<PolicyBasedBias, PlanLengthBias>);
} // namespace policy_testing
