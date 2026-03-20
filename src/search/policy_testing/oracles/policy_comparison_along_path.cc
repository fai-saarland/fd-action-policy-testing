#include "policy_comparison_along_path.h"

#include "../../plugins/plugin.h"
#include "../../task_utils/task_properties.h"
#include "../utils/custom_exceptions.h"
#include "../policies/remote_policy.h"
#include "../policy.h"
#include "../engines/testing_base_engine.h"

#include <cassert>
#include <utility>
#include <vector>

namespace policy_testing {
PolicyComparisonAlongPath::PolicyComparisonAlongPath(const plugins::Options &opts)
    : Oracle(opts),
      num_per_state(opts.get<int>("num_per_state")),
      run_prob(opts.get<double>("run_prob")),
      rng(0) {
}

void
PolicyComparisonAlongPath::add_options_to_feature(plugins::Feature &feature) {
    Oracle::add_options_to_feature(feature);
    feature.add_option<int>("num_per_state", "Number of policies run per state along the path, -1 for all available.", "-1");
    feature.add_option<double>("run_prob", "Only run portfolio policies on any state with given probability.", "1.0");
}

TestResult
PolicyComparisonAlongPath::test(Policy &policy, const State &state) {
    RemotePolicy *remote_policy = dynamic_cast<RemotePolicy *>(&policy);
    if (!remote_policy) {
        std::cerr << "Policy Comparison oracle can only be called with a remote policy." << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
    }

    std::vector<OperatorID> plan;
    std::vector<State> path;

    const auto [result_known, solved] = remote_policy->execute_get_plan_and_path(state, plan, path, 0);
    assert(!solved || plan.size() == path.size() - 1);
    // policy_costs.push_back(calculate_plan_cost(get_task(), plan));
    int remaining_path_cost = 0;
    for (int path_idx = path.size() - 1; path_idx >= 0; --path_idx) {
        if (!solved) {
            // Skip to first state
            path_idx = 0;
            remaining_path_cost = Policy::UNSOLVED;
        }

        bool run_on_state = rng.random() < run_prob ? true : false;
        std::vector<PolicyCost> policy_costs(remote_policy->get_num_models());
        for (int model = 1; run_on_state && (model < remote_policy->get_num_models() && (model <= num_per_state || num_per_state == -1)); ++model) {
            policy_costs[model] = remote_policy->compute_policy_cost(path[path_idx], model, remaining_path_cost);
            if (Policy::is_less(policy_costs[model], remaining_path_cost)) {
                if (remaining_path_cost == Policy::UNSOLVED) {
                    return TestResult(UNSOLVED_BUG_VALUE, policy_costs[model]); // TODO: incorrect upper_cost_bound
                }
                // std::cout << "Remaining path cost: " << remaining_path_cost << std::endl;
                // std::cout << "Portfolio policy cost: " << policy_costs[model] << std::endl;
                const int diff = remaining_path_cost - policy_costs[model];     // policy "model" at interm. state < remaining_cost => bug found (also in pool state)
    #ifndef NDEBUG
                if (debug) {
                    assert(confirm_bug(state, diff));
                }
    #endif
                return TestResult(diff, policy_costs[model]); // TODO: incorrect upper_cost_bound
            }
        }
        if (path_idx > 0) {
            remaining_path_cost += get_task()->get_operator_cost(plan[path_idx - 1].get_index(), false);
        }
    }

    return TestResult(0, remaining_path_cost);
}

class PolicyComparisonAlongPathFeature : public plugins::TypedFeature<Oracle, PolicyComparisonAlongPath> {
public:
    PolicyComparisonAlongPathFeature() : TypedFeature("policy_comparison_along_path") {
        PolicyComparisonAlongPath::add_options_to_feature(*this);
    }
};
static plugins::FeaturePlugin<PolicyComparisonAlongPathFeature> _plugin;
} // namespace policy_testing
