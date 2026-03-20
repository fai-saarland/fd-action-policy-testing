#pragma once

#include "../per_state_information.h"
#include "policy.h"
#include "policies/remote_policy.h"
#include "../task_proxy.h"
#include "engines/testing_base_engine.h"
#include "upper_bounds_base.h"
#include <deque>

namespace policy_testing {
struct CostSetRefExt {
    // upper bound for the optimal plan cost for all states in the set
    PolicyCost cost;
    // index of the state set in the state_sets list
    unsigned int index;

    explicit CostSetRefExt(PolicyCost cost) : cost(cost), index(0) {
    }
    CostSetRefExt(PolicyCost cost, unsigned int index) : cost(cost), index(index) {
    }

    bool operator==(const CostSetRefExt &rhs) const {
        return cost == rhs.cost;
    }

    std::strong_ordering operator<=>(const CostSetRefExt &rhs) const {
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

struct BoundResult {
    int model_idx;
    PolicyCost policy_result;
    PolicyCost bound_result;
};
/**
 * Class for maintaining upper bounds in a state space.
 *
 * For propagating bounds, all previous policy paths are cached. Multiple different policies are supported.
 * User is responsible for keeping the access-index consistent across different policies.
 */
class UpperBoundsExtended {
    friend class BoundMaintenancePctoOracle;

    using StateSet = std::vector<State>;

private:
    // Toggle propagation through (policy) paths.
    // If disabled, paths will not be stored in the first place.
    bool parent_propagation = false;

    // Upper bound on the cost of states
    PerStateInformation<PolicyCost> upper_bounds;

    // Whether unsolved plan (fragments) should be registered for propagation.
    bool register_unsolved = false;

    // Map associating pool states to bug reporting function.
    utils::HashMap<StateID, std::function<void(int)>> subsequent_bug_reporting;

    // Caches cost for policy execution
    PerStateInformation<std::vector<PolicyCost>> policy_cost_cache;

    // Stores parents wrt a policy in any given state; (parent, op_cost)
    PerStateInformation<std::vector<std::pair<State, int>>> cached_parents;

    // Set of improved bounds: (state, old_bound, new_bound)
    std::vector<std::tuple<State, int, int>> improved_bounds; // TODO: should this be a set or rather a vector?

    /**
     * Updates bound with new cost (if Policy::is_less).
     * @returns true, if upper bound actually updated, false otherwise
     */
    bool update_bound(State &state, PolicyCost new_cost);

    /**
     * Registers a path for later bound propagation.
     * Needs an operator sequence to determine respective added cost in propagation.
     *
     * @param model The policy model index for which the path should be stored.
     * @warning path and plan must fit together, method does not check whether respective operators are actually applicable
     */
    void register_path(const TaskProxy &task_proxy, std::vector<OperatorID> &plan, std::vector<State> &path, int model);

    // COST SETS METHODS

    // sets of states with same cost (contains only states from the pool)
    std::deque<StateSet> state_sets;
    // vector including the indices of the states sets, sorted with respect to cost TODO: POTENTIALLY NOT (contains only pool states)
    std::vector<CostSetRefExt> set_refs;
    // number of currently stored states in cost sets
    unsigned int cost_set_size = 0;
    // delayed states updates
    // tuples contain states, old cost value and new cost value
    std::vector<std::tuple<State, PolicyCost, PolicyCost>> delayed_cost_set_updates;

    /**
     * Add new (empty) state set of given @param cost.
     * Also update state references and keeps them sorted with respect to cost.
     * @return reference to the newly constructed state set.
     */
    StateSet &addNewCostSet(int cost) {
        assert(set_refs.size() == state_sets.size());
        assert(!std::binary_search(set_refs.cbegin(), set_refs.cend(), CostSetRefExt(cost)));
        unsigned int new_index = state_sets.size();
        state_sets.emplace_back();
        set_refs.emplace(std::upper_bound(set_refs.cbegin(), set_refs.cend(), CostSetRefExt(cost)), cost, new_index);
        return state_sets.back();
    }

    bool costSetExists(PolicyCost cost) {
        assert(set_refs.size() == state_sets.size());
        return std::binary_search(set_refs.cbegin(), set_refs.cend(), CostSetRefExt(cost));
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
        assert(std::binary_search(set_refs.cbegin(), set_refs.cend(), CostSetRefExt(cost)));
        return state_sets[std::lower_bound(set_refs.cbegin(), set_refs.cend(), CostSetRefExt(cost))->index];
    }

    /**
    * Get reference to an existing state using given @param ref.
    */
    [[nodiscard]] const StateSet &getCostSet(const CostSetRefExt &ref) const {
        return state_sets[ref.index];
    }

    /**
     * Adds a given @param state to the state set with cost @param cost. Constructs the state set if necessary.
     */
    void addState(const State &state, PolicyCost cost) {
        ++cost_set_size;
        auto it = std::lower_bound(set_refs.cbegin(), set_refs.cend(), CostSetRefExt(cost));
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
    void removeState(const State &state, PolicyCost cost) {
        assert(cost_set_size);
        --cost_set_size;
        auto &cost_set = getCostSetByCost(cost);
        auto it = std::find(cost_set.begin(), cost_set.end(), state);
        assert(it != cost_set.end());
        if (it == cost_set.end()) {
            std::cerr << "Trying to remove state with id " << std::string(state.get_id())
                      << " that is not contained in cost set for cost " << cost << std::endl;
            utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
        }
        std::swap(*it, cost_set.back());
        cost_set.pop_back();
    }

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

public:
    UpperBoundsExtended(int num_policies, bool parent_propagation, bool register_unsolved);

    PolicyCost get_cost_for_state(const State &state) {
        return upper_bounds[state];
    }

    /**
     * Computes the best possible cost value for the given state.
     * Uses cost given policy and existing cost bounds.
     *
     * @note Automatically updates bounds along policy path.
     */
    BoundResult run_policy(const TaskProxy &task_proxy, const State &state, RemotePolicy &remote_policy, int model = 0);

    /**
     * Calculates cost of a plan while also using UpperBoundsExtended for the bound_result field.
     *
     * @warning Plan must be valid!
     * @note If no model index is explicitly specified, path will not be stored for bound propagation.
     */
    BoundResult run_plan(const TaskProxy &task_proxy, const State &state, std::vector<OperatorID> plan, int model = -1);

    /**
     * Propagates and updates all improved_bounds through paths stored by cached_parents.
     *
     * @param policy Only propagate for policy at index policy. Value -1 updates for all policies.
     * @note parent_propagation must be set to true
     */
    void propagate_bounds(int policy = -1);

    /**
     * Allows to inject a new bound for some given state.
     * Useful for external bound information e.g. by Aras or other oracles not providing information through running policies.
     *
     * @note Will only update bound, if new_bound is better than the existing bound
     */
    bool inject_upper_bound(State &state, int new_bound);

    /**
     * Allows to register a pool bug for subsequent bug reporting.
     * If the bound of the pool state is improved at some point, it will be reported as a bug (if not known to be a bug already).
     */
    void register_subsequent_bug_reporting(PolicyTestingBaseEngine *engine, const State &pool_state, int policy_value);

    /**
     * Clears cached_paths; only necessary if caching all policy runs becomes too memory intensive.
     * Could then e.g. be called after an oracle invocation on a state.
     * Might be useful to only delete portfolio runs then, to still allow for later classification of pool bugs.
     *
     * NOTE: probably not possible given implementation of PerStateInformation; would require a split into PerStateInformation per Policy/"Entity"
     */
    // void clear_parent_information();


    /**
     * Insert a path where updates should be propagated.
     *
     * NOTE: would require some form of path handling apart from policy paths, e.g. with separate PerStateInformation or by not storing "runs" but just parents
     */
    // int update_along_path(std::vector<OperatorID> &plan, std::vector<State> &path, int model);
};

/**
 * Custom iterator over cost_set_refs.
 * Allows to start with a set with cost closest to a given cost, then alternates between picking the set with the
 * next higher and the next lower cost.
 */
class CostSetIteratorExt {
    const int start_cost;
    const std::vector<CostSetRefExt> *set_refs;

    using Iterator = std::vector<CostSetRefExt>::const_iterator;
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

    CostSetIteratorExt(int start_cost, const std::vector<CostSetRefExt> &set_refs, int end = false) :
        start_cost(start_cost),
        set_refs(&set_refs),
        forward_iterator(end ?
                         set_refs.cend() : std::lower_bound(set_refs.cbegin(), set_refs.cend(),
                                                            CostSetRefExt(start_cost))),
        forward_iterator_end(set_refs.cend()),
        backward_iterator(end ?
                          std::make_reverse_iterator(set_refs.cbegin()) : std::make_reverse_iterator(forward_iterator)),
        backward_iterator_end(std::make_reverse_iterator(set_refs.cbegin())),
        forward(forward_iterator != forward_iterator_end ||
                (forward_iterator == forward_iterator_end && backward_iterator == backward_iterator_end)) {}

    CostSetIteratorExt begin() {
        return {start_cost, *set_refs};
    }

    CostSetIteratorExt end() {
        return {start_cost, *set_refs, true};
    }

    const CostSetRefExt &operator*() const {
        if (forward) {
            assert(forward_iterator < forward_iterator_end);
            return *forward_iterator;
        } else {
            assert(backward_iterator < backward_iterator_end);
            return *backward_iterator;
        }
    }

    CostSetIteratorExt &operator++() {
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

    bool operator==(const CostSetIteratorExt &other) const = default;
};
}
