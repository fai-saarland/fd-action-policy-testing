#pragma once

#include "../oracle.h"
#include "../upper_bounds.h"
#include "../online_handler.h"
#include "../upper_bounds.h"
#include <queue>

namespace policy_testing {
/**
*  This Oracle uses multiple (different) portfolio policies to find bugs in a given policy under test.
*/
class PolicyComparisonOracle : public Oracle {
    friend class BoundMaintenancePctoOracle;

    // External oracle for evaluating quality bugs ONLY.
    std::shared_ptr<Oracle> qual_oracle;

public:
    explicit PolicyComparisonOracle(const plugins::Options &opts);
    static void add_options_to_feature(plugins::Feature &feature);

    void set_engine(PolicyTestingBaseEngine *engine);

    auto test_driver(Policy &policy, const PoolEntry &entry) -> TestResult override;

protected:
    TestResult test(Policy &policy, const State &state) override;
    auto test_impl(RemotePolicy &remote_policy, const State &state, int num_port_exec = -1, int amount_offset = 0) -> TestResult;


private:
    // Whether to store and improve upper bounds through oracle invocations.
    bool maintain_upper_bounds;
    std::shared_ptr<UpperBoundsExtended> upper_bounds;

    // Whether to perform majority vote instead of separate policy runs.
    bool majority_vote;

    // Offset execution of portfolio policies from path and pool state through random walk.
    int num_offset_runs;
    int amount_offset;

    // How many portfolio policies to execute per state.
    int num_per_state;

    // Whether to randomly sample from the policy models.
    bool random_sample = false;

    // Whether to manage a selection of best performing policies and use them.
    bool online_sample = false;
    bool online_pq = false;
    std::optional<OnlineHandler> online_handler;
    std::priority_queue<std::tuple<double, int>> best_policies;

    // Does not execute portfolio policies, if quality bug detected.
    // Can be useful as few quality bugs are found by PCTO anyways.
    bool ignore_quality_bugs;

    // Number of policies to execute per state along policy path.
    int num_per_path_state;
    // Probability whether to run portfolio policies on a path state.
    double run_prob;

    // Seeded random number generator.
    utils::RandomNumberGenerator rng;


    /**
     * Execute policy either using the upper bounds class (when maintaining upper bounds) or using the standard policy compute_policy_cost function.
     *
     * @returns BoundResult, as when using upper bounds, one needs to be able to differentiate between bound cost and policy cost.
     */
    auto execute_policy(const State &state, RemotePolicy &remote_policy, int model_index, int max_cost = -1) -> BoundResult;

    auto random_walk(const State &state, int depth) -> std::pair<State, int>;
    auto set_upper_bounds(std::shared_ptr<UpperBoundsExtended> upper_bounds) -> void;
};
} // namespace policy_testing
