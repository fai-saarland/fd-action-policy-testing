#pragma once

#include "../../search_engine.h"
#include "../cost_estimator.h"

#include <memory>
#include <optional>

namespace options {
class Options;
class OptionParser;
} // namespace options

namespace policy_testing {
class InternalPlannerPlanCostEstimator : public PlanCostEstimator {
public:
    enum class Configuration {
        ASTAR_LMCUT,
        LAMA_FIRST_ITERATION,
        LAMA_COMPLETE,
        EHC_FF,
        GBFS_FF,
        LAZY_GBFS_FF
    };

    explicit InternalPlannerPlanCostEstimator(const options::Options &opts);
    explicit InternalPlannerPlanCostEstimator(TestingEnvironment *env, bool continue_after_timeout);
    static void add_options_to_parser(options::OptionParser &parser);

    int compute_value(const State &state) override;

    /// like compute value but return DEAD_END if no plan has been found
    /// the oracle configuration needs to be complete for this to work!
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
    std::shared_ptr<SearchEngine> create(int max_time, const State &start_state, const State *goal_state = nullptr);

    utils::HashMap<StateID, int> trusted_values_cache;
    utils::HashMap<std::pair<StateID, StateID>, int> trusted_values_pairs_cache;
};
} // namespace policy_testing
