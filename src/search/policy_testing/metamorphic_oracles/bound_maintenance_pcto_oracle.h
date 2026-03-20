#pragma once

#include "metamorphic_oracle.h"
#include "../oracles/policy_comparison_oracle.h"
#include "../../evaluator.h"
#include "../upper_bounds.h"

#include <deque>
#include <iterator>

class State;
class Heuristic;

namespace policy_testing {
/**
 * Rough idea behind this comparison based metamorphic test oracle O:
 * - O maintains sets S^c with states s such that c >= h*(s) (c is an upper bound for the plan cost)
 * - For every new state t with policycost(t) given to O compare t with a feasible number of states s from sets S^c
 * - Observe that c_t := c - D(s,t) >= h*(s) + h*(t) - h*(s) = h*(t)  (since -D(s,t) >= h*(t) - h*(s) )
 * - Likewise c_s := policycost(t) - D(t,s) >= h*(t) + h*(s) - h*(t) = h*(s)  (since -D(t,s) >= h*(s) - h*(t) )
 * - If c_t < policycost(t), flag t as a bug. Likewise, if c_s < c, flag s as a bug.
 * - Put t into S^{min(policycost(t), c_t)} and move s to S^{c_s} if c_s < c
 */
class BoundMaintenancePctoOracle : public MetamorphicOracle {
    // friend class CompositeOracle;
    friend class CostSetIteratorExt;

    std::shared_ptr<PolicyComparisonOracle> pcto_oracle;

    std::shared_ptr<UpperBoundsExtended> upper_bounds;

    // upper bound on the cost of states
    // PerStateInformation<PolicyCost> upper_cost_bounds;

    // the number of old states to compare a new state to
    unsigned int max_state_comparisons;

    // switch indicating whether to perform lookahead search
    bool conduct_lookahead_search;

    // pass cost bounds to policy parent states
    bool update_parents;

    // the number of old states to compare a state to within lookahead search
    unsigned int max_lookahead_state_comparisons;

    // heuristic to be used in lookahead
    std::shared_ptr<Evaluator> lookahead_heuristic;

    // defer heuristic evaluation in lookahead_search
    bool deferred_evaluation;

public:
    enum class LookaheadComp {
        H, G_PLUS_H
    };
private:

    LookaheadComp lookahead_comp;

    // maximal number of state visits in each lookahead search invocation
    unsigned int max_lookahead_state_visits;

    // all tested states (including intermediate states if enabled)
    utils::HashSet<StateID> tested_states;

    // indicates that domain is unit cost and invertible, activates optimizations
    bool domain_unit_cost_and_invertible;

    /**
     * Update upper bound on plan cost.
     * Does not immediately update cost sets but registers it to be done in the future.
     * @param s states to update cost of.
     * @param old_cost old cost (consistent with cost set entry).
     * @param new_cost new cost.
     * @note does not update to new_cost if a smaller cost bound is already known.
     */
    void update_cost(const State &s, PolicyCost old_cost, PolicyCost new_cost);

    /**
     * Performs delayed state set updates.
     * Every state is removed from states set of cost old_cost and put into state sets with cost new_cost.
     */
    void reorder_state_sets();

    /**
     * Performs delayed state set updates.
     * Every state is removed from states set of cost old_cost and put into state sets with cost new_cost.
     * Additional, if update_parents is set, call update_parent_cost on all updated states and,
     * if necessary, reorder again.
     */
    void reorder_state_sets_with_parent_updates(Policy &policy);

    /**
     * Infers upper bound on h*(s) via comparisons with stored states.
     * @param policy the evaluated policy.
     * @param s the state
     * @return an upper bound b such that h*(s) <= b (or Policy::UNSOLVED if no such bound can be found)
     * @note returns 0 if s is a goal state and b is at most policycost(s) if policycost(s) is already known.
     * Does NOT actually execute policy.
     * @warning may reorder state sets
     */
    PolicyCost infer_upper_bound(Policy &policy, const State &s);

    /**
     * Uses delayed_cost_updated.
     */
    void update_parent_cost(Policy &policy, const State &s);

    PolicyCost lookahead_search(Policy &policy, const State &s, unsigned int max_state_visits);

    BugValue test_impl(Policy &policy, const State &state, bool local_test, bool lookahead);

protected:
    void initialize() override;

    void set_engine(PolicyTestingBaseEngine *engine) override;

    TestResult test(Policy &, const State &) override;

public:
    explicit BoundMaintenancePctoOracle(const plugins::Options &opts);

    static void add_options_to_feature(plugins::Feature &feature);

    TestResult test_driver(Policy &policy, const PoolEntry &pool_entry) override;

    void add_external_cost_bound(Policy &policy, const State &s, PolicyCost cost_bound) override;
};
} // namespace policy_testing
