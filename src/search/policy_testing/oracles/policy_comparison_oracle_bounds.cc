#include "policy_comparison_oracle_bounds.h"

#include "../../plugins/plugin.h"
#include "../../task_utils/task_properties.h"
#include "../utils/custom_exceptions.h"
#include "../policies/remote_policy.h"
#include "../policy.h"

#include <cassert>
#include <utility>
#include <vector>
#include <thread>
#include <chrono>

namespace policy_testing {
PolicyComparisonOracleBounds::PolicyComparisonOracleBounds(const plugins::Options &opts)
    : Oracle(opts),
      upper_bounds(std::make_shared<UpperBoundsExtended>(30, opts.get<bool>("parent_propagation"), opts.get<bool>("register_unsolved"))), // TODO: number of policies must not be fixed!
      ignore_quality_bugs(opts.get<bool>("ignore_quality_bugs")) {
}

PolicyComparisonOracleBounds::~PolicyComparisonOracleBounds() {
    // delete upper_bounds;
}

void
PolicyComparisonOracleBounds::add_options_to_feature(plugins::Feature &feature) {
    Oracle::add_options_to_feature(feature);
    feature.add_option<bool>("ignore_quality_bugs", "If set, quality bugs are ignored.", "false");
    feature.add_option<bool>("parent_propagation", "If set, improved upper bounds are propagated through policy paths.", "false");
    feature.add_option<bool>("register_unsolved", "If set, also registers paths that do not reach a goal.", "false");
}

TestResult
PolicyComparisonOracleBounds::test(Policy &policy, const State &state) {
    RemotePolicy *remote_policy = dynamic_cast<RemotePolicy *>(&policy);
    if (!remote_policy) {
        std::cerr << "Policy Comparison oracle can only be called with a remote policy." << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
    }

    std::vector<PolicyCost> policy_costs;     // Index 0 is "policy under test"
    policy_costs.push_back(upper_bounds->run_policy(get_task_proxy(), state, *remote_policy, 0).policy_result); // IMPORTANT: use policy_result here, as this is policy under test
    if (ignore_quality_bugs && policy_costs[0] < 0) {
        return TestResult(0, policy_costs[0]);
    }

    for (int model = 1; model < remote_policy->get_num_models(); ++model) {
        policy_costs.push_back(upper_bounds->run_policy(get_task_proxy(), state, *remote_policy, model).bound_result);
        // assert(policy_costs[model] == remote_policy->compute_policy_cost(state, model));
#ifndef NDEBUG
        if (debug) {
            std::cout << "Policy Cost " << model << ": " << policy_costs[model] << std::endl;
        }
#endif
        if (Policy::is_less(policy_costs[model], policy_costs[0])) {
            if (policy_costs[0] == Policy::UNSOLVED) {
                return TestResult(UNSOLVED_BUG_VALUE, policy_costs[model]);
            }
            const int diff = policy_costs[0] - policy_costs[model];     // policy "model" < policy 0 => bug in policy 0 (under test)
#ifndef NDEBUG
            if (debug) {
                assert(confirm_bug(state, diff));
            }
#endif
            return TestResult(diff, policy_costs[model]);
        }
    }

    // Register pool state for subsequent bug reporting
    if (!engine->is_known_bug(state)) {
        upper_bounds->register_subsequent_bug_reporting(engine, state, policy_costs[0]);
    }

    return TestResult(0, policy_costs[0]);
}

void PolicyComparisonOracleBounds::set_upper_bounds(std::shared_ptr<UpperBoundsExtended> upper_bounds) {
    // if (upper_bounds == PolicyComparisonOracleBounds::upper_bounds) {
    //     return;
    // }
    //delete PolicyComparisonOracleBounds::upper_bounds;
    PolicyComparisonOracleBounds::upper_bounds = upper_bounds;
}

std::shared_ptr<UpperBoundsExtended> PolicyComparisonOracleBounds::get_upper_bounds() {
    return upper_bounds;
}

class PolicyComparisonOracleBoundsFeature : public plugins::TypedFeature<Oracle, PolicyComparisonOracleBounds> {
public:
    PolicyComparisonOracleBoundsFeature() : TypedFeature("policy_comparison_oracle_bounds") {
        PolicyComparisonOracleBounds::add_options_to_feature(*this);
    }
};
static plugins::FeaturePlugin<PolicyComparisonOracleBoundsFeature> _plugin;
} // namespace policy_testing
