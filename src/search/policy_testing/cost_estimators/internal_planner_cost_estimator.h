#pragma once

#include "../../search_algorithm.h"
#include "../cost_estimator.h"

#include <memory>
#include <optional>

namespace policy_testing {
class InternalPlannerPlanCostEstimator : public PlanCostEstimator {
public:
    enum class Configuration {
        ASTAR_LMCUT,
        EHC_FF,
    };

    explicit InternalPlannerPlanCostEstimator(const plugins::Options &opts);
    explicit InternalPlannerPlanCostEstimator(TestingEnvironment *env, bool continue_after_timeout);
    static void add_options_to_feature(plugins::Feature &feature);

    int compute_value(const State &state) override;

    /// like compute value but return DEAD_END if no plan has been found
    /// the search configuration needs to be complete for this to work!
    int compute_trusted_value(const State &state, const State *goal_state = nullptr);

    /// wrapper around compute_trusted_value, caching call results
    int compute_trusted_value_with_cache(const State &start_state, const State *goal_state = nullptr);

    /**
     * Run planner configuration to compute plan. Returns true if plan was
     * found, false otherwise. May throw OutOfResourceException if runtime or
     * memory limits are exceeded.
     **/
    bool run_planner(std::vector<OperatorID> &plan, const State &start_state, const State *goal_state = nullptr);

    const Configuration configuration_;
    const bool print_output_;
    const bool print_plan_;
    const int max_planner_time;
    const bool continue_after_time_out;

private:
    /** @brief attempt to create a search engine with the given max search time and initial state and
     * (if provided) goal state.
     * Return nullptr if engine the creation failed.
     */
    std::shared_ptr<SearchAlgorithm> create(double max_time, const State &start_state, const State *goal_state = nullptr);

    utils::HashMap<StateID, int> trusted_values_cache;
    utils::HashMap<std::pair<StateID, StateID>, int> trusted_values_pairs_cache;
};
} // namespace policy_testing
