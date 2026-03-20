#include "policy_comparison_oracle_online.h"

#include "../../plugins/plugin.h"
#include "../../task_utils/task_properties.h"
#include "../utils/custom_exceptions.h"
#include "../policies/remote_policy.h"
#include "../policy.h"

#include <cassert>
#include <utility>
#include <vector>

namespace policy_testing {
PolicyComparisonOracleOnline::PolicyComparisonOracleOnline(const plugins::Options &opts)
    : Oracle(opts),
      num_port_policies(opts.get<int>("num_port_policies")),
      rng(0) {
}

void
PolicyComparisonOracleOnline::add_options_to_feature(plugins::Feature &feature) {
    Oracle::add_options_to_feature(feature);
    feature.add_option<int>("num_port_policies", "Number of portfolio policies to be executed on each state.", "3");
}

TestResult
PolicyComparisonOracleOnline::test(Policy &policy, const State &state) {
    RemotePolicy *remote_policy = dynamic_cast<RemotePolicy *>(&policy);
    if (!remote_policy) {
        std::cerr << "Policy Comparison oracle can only be called with a remote policy." << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
    }
    assert(remote_policy->get_num_models() > num_port_policies);

    if (bugs_found_by_policy.empty()) {
        bugs_found_by_policy = std::vector<std::pair<int, int>>(remote_policy->get_num_models(), std::pair<int, int>(1, 0)); // TODO: definitely inits pair values to zero?
    }
    assert(bugs_found_by_policy.size() == remote_policy->get_num_models());

    // int i = 0;
    // std::cout << "All ratios:" << std::endl;
    // for (auto [num_tests, num_bugs] : bugs_found_by_policy) {
    //     double ratio = (double)num_bugs / (double)num_tests;
    //     std::cout << "(" << i << "," << ratio << ")";
    //     ++i;
    // }
    // std::cout << std::endl;
    // std::cout << "In most_found:" << std::endl;
    // for (auto [port_num, ratio] : most_found) {
    //     std::cout << "(" << port_num << "," << ratio << ")";
    // }
    // std::cout << std::endl;

    // std::cout << "Most found size: " << most_found.size() << std::endl;

    bool choose_best_performing = rng.random(2); // 50/50 decision

    // std::cout << "Choose best performing: " << choose_best_performing << std::endl;

    std::vector<std::pair<int, PolicyCost>> policy_costs; // Policy under test has index 0; (model_index, PolicyCost)
    policy_costs.emplace_back(0, remote_policy->compute_policy_cost(state, 0));
    std::map<int, double> cp_most_found = most_found;

    auto it = most_found.begin();
    for (int policy_num = 1; policy_num <= num_port_policies; ++policy_num) {
        int port_model;
        int port_cost;
        if (choose_best_performing) {
            // for (auto [port_num, ratio] : most_found) {
            //     std::cout << "(" << port_num << "," << ratio << ")" << std::endl;
            // }

            if (it == most_found.end()) {
                // std::cout << "In Queue empty." << std::endl;
                port_model = rng.random(remote_policy->get_num_models() - 1) + 1;
                // std::cout << "Empty Queue, chosen model: " << port_model << std::endl;
            } else {
                // std::cout << "Queue not empty." << std::endl;
                port_model = it->first;
                ++it;
                // std::cout << "Chosen model: " << port_model << std::endl;
            }
        } else {
            port_model = rng.random(remote_policy->get_num_models() - 1) + 1;
        }
        port_cost = remote_policy->compute_policy_cost(state, port_model);
        bugs_found_by_policy[port_model].first++;

        // int i = 0;
        // std::cout << "All ratios after run:" << std::endl;
        // for (auto [num_tests, num_bugs] : bugs_found_by_policy) {
        //     double ratio = (double)num_bugs / (double)num_tests;
        //     std::cout << "(" << i << "," << ratio << ")";
        //     ++i;
        // }
        // std::cout << std::endl;
        // std::cout << "In most_found after run:" << std::endl;
        // for (auto [port_num, ratio] : most_found) {
        //     std::cout << "(" << port_num << "," << ratio << ")";
        // }
        // std::cout << std::endl;

        update_ratios(port_model);

        // std::cout << "Bugs found by policy " << port_model << ": " << bugs_found_by_policy[port_model].second << " in " << bugs_found_by_policy[port_model].first << " runs" << std::endl;

#ifndef NDEBUG
        if (debug) {
            std::cout << "Policy Cost " << port_model << ": " << port_cost << std::endl;
        }
#endif

        // testing for bug and reporting
        if (Policy::is_less(port_cost, policy_costs[0].second)) {
            // found bug here
            bugs_found_by_policy[port_model].second++;

            update_ratios(port_model);

            if (policy_costs[0].second == Policy::UNSOLVED) {
                return TestResult(UNSOLVED_BUG_VALUE, port_cost);
            }
            const int diff = policy_costs[0].second - port_cost;
#ifndef NDEBUG
            if (debug) {
                assert(confirm_bug(state, diff));
            }
#endif
            return TestResult(diff, port_cost);
        }
    }

    return TestResult(0, policy_costs[0].second);
}

void PolicyComparisonOracleOnline::update_ratios(int port_model) {
    // Update most found with best performing policies
    double bugs_to_runs_ratio = ((double)bugs_found_by_policy[port_model].second) / ((double)bugs_found_by_policy[port_model].first);
    // std::cout << "Bugs to run ratio model " << port_model << ": " << bugs_to_runs_ratio << std::endl;
    if (most_found.size() < num_port_policies) {
        most_found.insert_or_assign(port_model, bugs_to_runs_ratio);     // (model_index, #bugs/#runs)
        // std::cout << "Only pushed new entry into most_found." << std::endl;
    } else {
        if (most_found.contains(port_model)) {
            // std::cout << "most_found already contains model" << std::endl;
            most_found.insert_or_assign(port_model, bugs_to_runs_ratio);
        } else {
            //std::cout << "most_found not yet contains model" << std::endl;
            int worst_ratio_model = -1;
            double worst_ratio = 1.0;
            for (auto &map_pair : most_found) {
                double bugs_to_runs_ration_other = map_pair.second;
                // std::cout << "compare to other ration model " << map_pair.first << ": " << bugs_to_runs_ration_other << std::endl;
                if (bugs_to_runs_ration_other < worst_ratio) {
                    // Update worst_ratio if new worst found
                    worst_ratio = bugs_to_runs_ration_other;
                    worst_ratio_model = map_pair.first;
                }
            }
            if (bugs_to_runs_ratio > worst_ratio_model) {
                // std::cout << "Map update" << std::endl;
                most_found.erase(worst_ratio_model);
                most_found.insert_or_assign(port_model, bugs_to_runs_ratio);
            }
        }
        // std::cout << "Popped and pushed new entry into most_found." << std::endl;
    }
}

class PolicyComparisonOracleOnlineFeature : public plugins::TypedFeature<Oracle, PolicyComparisonOracleOnline> {
public:
    PolicyComparisonOracleOnlineFeature() : TypedFeature("policy_comparison_oracle_online") {
        PolicyComparisonOracleOnline::add_options_to_feature(*this);
    }
};
static plugins::FeaturePlugin<PolicyComparisonOracleOnlineFeature> _plugin;
} // namespace policy_testing
