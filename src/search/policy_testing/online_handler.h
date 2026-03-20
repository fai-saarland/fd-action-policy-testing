#pragma once

#include "oracles/policy_comparison_oracle.h"
#include "policy.h"

namespace policy_testing {
class OnlineHandler {
private:
    // Number of policies to be executed per state.
    int num_per_state;
    // First: num times policy was run; Second: num bugs found through policy
    std::vector<std::pair<int, int>> bugs_found_by_policy;
    // A map containing ratio numbers for the best portfolio policies.
    std::map<int, double> most_found;

public:
    explicit OnlineHandler() {
        num_per_state = 1;
    }

    explicit OnlineHandler(int best_size) {
        num_per_state = best_size;
    }

    void run_no_bug(int port_model) {
        if (bugs_found_by_policy.size() <= port_model) {
            bugs_found_by_policy.resize(port_model + 1, std::pair<int, int>(1, 0));
        }
        bugs_found_by_policy[port_model].first++;
        update_ratios(port_model);
    }

    void run_found_bug(int port_model) {
        if (bugs_found_by_policy.size() <= port_model) {
            bugs_found_by_policy.resize(port_model + 1, std::pair<int, int>(1, 0));
        }
        bugs_found_by_policy[port_model].first++;
        bugs_found_by_policy[port_model].second++;
        update_ratios(port_model);
    }

    int sample_from_best(utils::RandomNumberGenerator &rng) {
        int nth_element = rng.random(most_found.size());
        int i = 0;
        for (auto most_found_entry : most_found) {
            if (i == nth_element) {
                return most_found_entry.first;
            }
            i++;
        }
        std::cerr << "Inconsistency between most_found size and iteration over most found." << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
    }

    /**
     * Samples from all available policies in remote_policy without policy under test at index 0.
     */
    int sample_from_all(RemotePolicy &remote_policy, utils::RandomNumberGenerator &rng) {
        return rng.random(remote_policy.get_num_models() - 1) + 1;
    }

    int take_from_best(int nth_element, RemotePolicy &remote_policy, utils::RandomNumberGenerator &rng) {
        if (most_found.size() == 0) {
            return sample_from_all(remote_policy, rng);
        }
        nth_element = nth_element % most_found.size();
        int i = 0;
        for (auto most_found_entry : most_found) {
            if (i == nth_element) {
                return most_found_entry.first;
            }
            i++;
        }
        std::cerr << "Inconsistency between most_found size and iteration over most found." << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
    }

    /**
     * Returns the number of times policy model_idx was run.
     */
    int get_num_runs(int model_idx) {
        return bugs_found_by_policy[model_idx].first;
    }

    /**
     * Returns the number of bugs found by policy model_idx.
     */
    int get_num_bugs(int model_idx) {
        return bugs_found_by_policy[model_idx].second;
    }

    void print_current_best() {
        int i = 0;
        std::cout << "All data:" << std::endl;
        for (auto [num_tests, num_bugs] : bugs_found_by_policy) {
            std::cout << "(" << i << "," << num_bugs << "," << num_tests << ")";
            ++i;
        }
        std::cout << std::endl;
        i = 0;
        std::cout << "All ratios:" << std::endl;
        for (auto [num_tests, num_bugs] : bugs_found_by_policy) {
            double ratio = (double)num_bugs / (double)num_tests;
            std::cout << "(" << i << "," << ratio << ")";
            ++i;
        }
        std::cout << std::endl;
        std::cout << "In most_found:" << std::endl;
        for (auto [port_num, ratio] : most_found) {
            std::cout << "(" << port_num << "," << ratio << ")";
        }
        std::cout << std::endl;
    }

private:
    /**
     * Updated the most_found map accounting for the new ratio of port_model.
     */
    void update_ratios(int port_model) {
        // Update most found with best performing policies
        double bugs_to_runs_ratio = ((double)bugs_found_by_policy[port_model].second) / ((double)bugs_found_by_policy[port_model].first);
        // If most_found not yet filled, insert always.
        if (most_found.size() < num_per_state) {
            most_found.insert_or_assign(port_model, bugs_to_runs_ratio);     // (model_index, #bugs/#runs)
            return;
        }
        // If port_model already in most_found, update. (NOTE: never removes if ratio got worse)
        if (most_found.contains(port_model)) {
            most_found.insert_or_assign(port_model, bugs_to_runs_ratio);
            return;
        }

        // If better ratio, replace entry with worst ratio.
        int worst_ratio_model = -1;
        double worst_ratio = 1.0;
        for (auto &map_pair : most_found) {
            double bugs_to_runs_ration_other = map_pair.second;
            if (bugs_to_runs_ration_other < worst_ratio) {
                // Update worst_ratio if new worst found
                worst_ratio = bugs_to_runs_ration_other;
                worst_ratio_model = map_pair.first;
            }
        }

        if (bugs_to_runs_ratio > worst_ratio) {
            most_found.erase(worst_ratio_model);
            most_found.insert_or_assign(port_model, bugs_to_runs_ratio);
        }
    }
};
}
