#pragma once

#include "numeric_dominance_oracle.h"
#include "../../evaluator.h"

#include <deque>
#include <iterator>

class State;
class Heuristic;

namespace policy_testing {
struct CostSetRef {
    // upper bound for the optimal plan cost for all states in the set
    PolicyCost cost;
    // index of the state set in the state_sets list
    unsigned int index;

    explicit CostSetRef(PolicyCost cost) : cost(cost), index(0) {}
    CostSetRef(PolicyCost cost, unsigned int index) : cost(cost), index(index) {}

    bool operator==(const CostSetRef &rhs) const {
        return cost == rhs.cost;
    }

    std::strong_ordering operator<=>(const CostSetRef &rhs) const {
        if (cost == Policy::UNSOLVED) {
            if (rhs.cost == Policy::UNSOLVED) {
                return std::strong_ordering::equal;
            } else {
                return std::strong_ordering::greater;
            }
        } else {
            if (rhs.cost == Policy::UNSOLVED) {
                return std::strong_ordering::less;
            } else {
                return cost <=> rhs.cost;
            }
        }
    }
};

/**
 * Rough idea behind this comparison based metamorphic test oracle O:
 * - O maintains sets S^c with states s such that c >= h*(s) (c is an upper bound for the plan cost)
 * - For every new state t with policycost(t) given to O compare t with a feasible number of states s from sets S^c
 * - Observe that c_t := c - D(s,t) >= h*(s) + h*(t) - h*(s) = h*(t)  (since -D(s,t) >= h*(t) - h*(s) )
 * - Likewise c_s := policycost(t) - D(t,s) >= h*(t) + h*(s) - h*(t) = h*(s)  (since -D(t,s) >= h*(s) - h*(t) )
 * - If c_t < policycost(t), flag t as a bug. Likewise, if c_s < c, flag s as a bug.
 * - Put t into S^{min(policycost(t), c_t)} and move s to S^{c_s} if c_s < c
 */
class IterativeImprovementOracle : public NumericDominanceOracle {
    friend class CompositeOracle;
    friend class CostSetIterator;
    using StateSet = std::vector<State>;

    // sets of states with same cost (contains only states from the pool)
    std::deque<StateSet> state_sets;
    // vector including the indices of the states sets, sorted with respect to cost (contains only pool states)
    std::vector<CostSetRef> set_refs;
    // number of currently stored states in cost sets
    unsigned int cost_set_size = 0;

    // delayed states updates
    // tuples contain states, old cost value and new cost value
    std::vector<std::tuple<State, PolicyCost, PolicyCost>> delayed_cost_set_updates;

    // upper bound on the cost of states
    PerStateInformation<PolicyCost> upper_cost_bounds;

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
     * Add new (empty) state set of given @param cost.
     * Also update state references and keeps them sorted with respect to cost.
     * @return reference to the newly constructed state set.
     */
    StateSet &addNewCostSet(int cost) {
        assert(set_refs.size() == state_sets.size());
        assert(!std::binary_search(set_refs.cbegin(), set_refs.cend(), CostSetRef(cost)));
        unsigned int new_index = state_sets.size();
        state_sets.emplace_back();
        set_refs.emplace(std::upper_bound(set_refs.cbegin(), set_refs.cend(), CostSetRef(cost)), cost, new_index);
        return state_sets.back();
    }

    bool costSetExists(PolicyCost cost) {
        assert(set_refs.size() == state_sets.size());
        return std::binary_search(set_refs.cbegin(), set_refs.cend(), CostSetRef(cost));
    }

    bool stateIsInCostSet(const State &state, PolicyCost cost) {
        if (!costSetExists(cost)) {
            return false;
        }
        auto &cost_set = getCostSetByCost(cost);
        auto it = std::find(cost_set.begin(), cost_set.end(), state);
        return it != cost_set.end();
    }

    /**
    * Get reference to an existing state set of given @param cost.
     * @warning it is the callers responsibility to guarantee that such a set exists.
    */
    StateSet &getCostSetByCost(int cost) {
        assert(set_refs.size() == state_sets.size());
        assert(std::binary_search(set_refs.cbegin(), set_refs.cend(), CostSetRef(cost)));
        return state_sets[std::lower_bound(set_refs.cbegin(), set_refs.cend(), CostSetRef(cost))->index];
    }

    /**
    * Get reference to an existing state using given @param ref.
    */
    [[nodiscard]] const StateSet &getCostSet(const CostSetRef &ref) const {
        return state_sets[ref.index];
    }

    /**
     * Adds a given @param state to the state set with cost @param cost. Constructs the state set if necessary.
     */
    void addState(const State &state, PolicyCost cost) {
        ++cost_set_size;
        auto it = std::lower_bound(set_refs.cbegin(), set_refs.cend(), CostSetRef(cost));
        if (it != set_refs.end() && it->cost == cost) {
            // state set already exists
            state_sets[it->index].push_back(state);
        } else {
            // new cost set needs to be constructed
            addNewCostSet(cost).push_back(state);
        }
    }

    /**
     * Adds states to respective cost sets.
     * @param states vector of pairs of states and cost values.
     */
    void addStates(const std::vector<std::pair<State, PolicyCost>> &add_list) {
        for (const auto &[s, c] : add_list) {
            addState(s, c);
        }
    }

    /**
     * @brief Remove states from respective cost set.
     * @warning the state must exists in the corresponding sets.
     */
    void removeState(const State &state, PolicyCost cost);

    /**
     * Removes states from respective cost sets.
     * @param states vector of pairs of states and cost values.
     *  @warning the states must exists in the corresponding sets.
     */
    void removeStates(const std::vector<std::pair<State, PolicyCost>> &remove_list) {
        for (const auto &[s, c] : remove_list) {
            removeState(s, c);
        }
    }

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

    void update_parent_cost(Policy &policy, const State &s);

    PolicyCost lookahead_search(Policy &policy, const State &s, unsigned int max_state_visits);

    BugValue test_impl(Policy &policy, const State &state, bool local_test, bool lookahead);

protected:
    void initialize() override;

    TestResult test(Policy &, const State &) override;

public:
    explicit IterativeImprovementOracle(const plugins::Options &opts);

    static void add_options_to_feature(plugins::Feature &feature);

    TestResult test_driver(Policy &policy, const PoolEntry &pool_entry) override;

    void add_external_cost_bound(Policy &policy, const State &s, PolicyCost cost_bound) override;
};


/**
 * Custom iterator over cost_set_refs.
 * Allows to start with a set with cost closest to a given cost, then alternates between picking the set with the
 * next higher and the next lower cost.
 */
class CostSetIterator {
    const int start_cost;
    const std::vector<CostSetRef> *set_refs;

    using Iterator = std::vector<CostSetRef>::const_iterator;
    using ReverseIterator = std::reverse_iterator<Iterator>;
    // for iterating over the sets with cost above start cost, initialized according to start_cost
    Iterator forward_iterator;
    const Iterator forward_iterator_end;
    // for iterating over the sets with cost below start cost, starts one position left of forward_iterator
    ReverseIterator backward_iterator;
    const ReverseIterator backward_iterator_end;

    // flag indicating whether to dereference from the forward or from the backward iterator
    // only set to false if backward iterator has next value
    // (guarantees forward==true in end state for combined iterator)
    bool forward;

public:

    CostSetIterator(int start_cost, const std::vector<CostSetRef> &set_refs, int end = false) :
        start_cost(start_cost),
        set_refs(&set_refs),
        forward_iterator(end ?
                         set_refs.cend() : std::lower_bound(set_refs.cbegin(), set_refs.cend(),
                                                            CostSetRef(start_cost))),
        forward_iterator_end(set_refs.cend()),
        backward_iterator(end ?
                          std::make_reverse_iterator(set_refs.cbegin()) : std::make_reverse_iterator(forward_iterator)),
        backward_iterator_end(std::make_reverse_iterator(set_refs.cbegin())),
        forward(forward_iterator != forward_iterator_end ||
                (forward_iterator == forward_iterator_end && backward_iterator == backward_iterator_end)) {}

    CostSetIterator begin() {
        return {start_cost, *set_refs};
    }

    CostSetIterator end() {
        return {start_cost, *set_refs, true};
    }

    const CostSetRef &operator*() const {
        if (forward) {
            assert(forward_iterator < forward_iterator_end);
            return *forward_iterator;
        } else {
            assert(backward_iterator < backward_iterator_end);
            return *backward_iterator;
        }
    }

    CostSetIterator &operator++() {
        if (forward) {
            if (forward_iterator != forward_iterator_end) {
                ++forward_iterator;
            }
            // potentially switch and make sure that forward==true holds in end state of combined iterator
            if (backward_iterator != backward_iterator_end) {
                forward = false;
            }
        } else {
            if (backward_iterator != backward_iterator_end) {
                ++backward_iterator;
            }
            // potentially switch and make sure that forward==true holds in end state of combined iterator
            if (backward_iterator == backward_iterator_end || forward_iterator != forward_iterator_end) {
                forward = true;
            }
        }
        return *this;
    }

    bool operator==(const CostSetIterator &other) const = default;
};
} // namespace policy_testing
