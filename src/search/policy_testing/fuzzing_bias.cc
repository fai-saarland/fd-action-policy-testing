#include "fuzzing_bias.h"

#include "../plugin.h"

namespace policy_testing {
PolicyBasedBias::PolicyBasedBias(const options::Options &opts) :
    policy(opts.get<std::shared_ptr<Policy>>("policy")),
    horizon(opts.get<unsigned int>("horizon")) {
    register_sub_component(policy.get());
}

void
PolicyBasedBias::add_options_to_parser(options::OptionParser &parser) {
    parser.add_option<std::shared_ptr<Policy>>("policy");
    parser.add_option<unsigned int>("horizon",
                                    "number of policy steps to consider in bias computation; choose 0 to set no limit",
                                    "100");
}

bool PolicyBasedBias::policy_is_known_to_fail(const State &s, unsigned int budget) {
    return policy->compute_lower_policy_cost_bound(s, get_step_limit(budget)).first == Policy::UNSOLVED;
}

static PluginTypePlugin<FuzzingBias> _plugin_type("FuzzingBias", "");
} // namespace policy_testing
