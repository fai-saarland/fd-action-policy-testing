#include "fuzzing_bias.h"

#include "policies/remote_policy.h"
#include "../plugin.h"

namespace policy_testing {
PolicyBasedBias::PolicyBasedBias(const options::Options &opts) :
    policy(opts.contains("policy") ? opts.get<std::shared_ptr<Policy>>("policy"): nullptr),
    horizon(opts.get<unsigned int>("horizon")) {
    if (RemotePolicy::connection_established()) {
        utils::g_log << "No additional policy specification found. "
            "Assuming global remote_policy with standard configuration." << std::endl;
        policy = RemotePolicy::get_global_default_policy();
    } else {
        if (!policy) {
            std::cerr << "You need to provide a policy." << std::endl;
            utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
        }
    }
    register_sub_component(policy.get());
}

void
PolicyBasedBias::add_options_to_parser(options::OptionParser &parser) {
    parser.add_option<std::shared_ptr<Policy>>("policy", "", options::OptionParser::NONE);
    parser.add_option<unsigned int>("horizon",
                                    "number of policy steps to consider in bias computation; choose 0 to set no limit",
                                    "50");
}

bool PolicyBasedBias::policy_is_known_to_fail(const State &s, unsigned int budget) {
    return policy->compute_lower_policy_cost_bound(s, get_step_limit(budget)).first == Policy::UNSOLVED;
}

static PluginTypePlugin<FuzzingBias> _plugin_type("FuzzingBias", "");
} // namespace policy_testing
