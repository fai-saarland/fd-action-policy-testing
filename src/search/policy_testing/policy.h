#pragma once

#include "../operator_id.h"
#include "../per_state_information.h"
#include "component.h"
#include "utils.h"

#include <memory>

#include <iostream>
#include <fstream>
#include <optional>

class Evaluator;

namespace policy_testing {
using PolicyCost = int;

/**
 * Inserts an element into a sorted vector if it is not already present and keeps the vector sorted.
 * @param vec
 * @param elem
 */
void insert_sorted(std::vector<StateID> &vec, StateID elem) {
    auto it = std::lower_bound(vec.begin(), vec.end(), elem);
    if (it != vec.end() && *it == elem) {
        // elem already exists in vector
        return;
    }
    // note that std::upper_bound behaves like std::lower_bound if element is not in vector
    vec.insert(it, elem);
}

class RunningPolicyCacheWriter {
    std::ofstream out;
public:
    explicit RunningPolicyCacheWriter(const std::string &path) {
        out.open(path);
    }
    ~RunningPolicyCacheWriter() = default;

    // a state and the chosen operator id is written to a single line of space separated integers
    // consisting of the operator id (first) and the state variables
    void write(const State &state, int op) {
        out << op;
        state.unpack();
        for (int val : state.get_unpacked_values()) {
            out << " " << val;
        }
        out << std::endl;
    }
};

class Policy : public TestingBaseComponent {
public:
    inline static constexpr PolicyCost UNSOLVED = -1;
    inline static constexpr PolicyCost UNKNOWN = -2;
    inline static constexpr const OperatorID NO_OPERATOR = OperatorID(-1);

    struct RunResult {
        // the run of the policy has not been aborted
        const bool complete = false;
        // the policy solves the tested state (it reaches a goal state and does not get stuck or diverges)
        // only relevant if complete is true
        const bool solves_state = false;
        RunResult(bool complete, bool solved) : complete(complete), solves_state(solved) {}
    };


    inline static PolicyCost min_cost(PolicyCost a, PolicyCost b) {
        assert(a == UNKNOWN || a == UNSOLVED || a >= 0);
        assert(b == UNKNOWN || b == UNSOLVED || b >= 0);
        if (a == UNKNOWN || b == UNKNOWN) {
            return UNKNOWN;
        } else if (a == UNSOLVED) {
            return b;
        } else if (b == UNSOLVED) {
            return a;
        } else {
            assert(a >= 0 && b >= 0);
            return std::min(a, b);
        }
    }

    inline static PolicyCost add_cost(PolicyCost a, PolicyCost b) {
        assert(a == UNKNOWN || a == UNSOLVED || a >= 0);
        assert(b == UNKNOWN || b == UNSOLVED || b >= 0);
        if (a == UNKNOWN || b == UNKNOWN) {
            return UNKNOWN;
        } else if (a == UNSOLVED || b == UNSOLVED) {
            return UNSOLVED;
        } else {
            assert(a >= 0 && b >= 0);
            return a + b;
        }
    }

    [[maybe_unused]] inline static PolicyCost max_cost(PolicyCost a, PolicyCost b) {
        assert(a == UNKNOWN || a == UNSOLVED || a >= 0);
        assert(b == UNKNOWN || b == UNSOLVED || b >= 0);
        if (a == UNKNOWN || b == UNKNOWN) {
            return UNKNOWN;
        } else if (a == UNSOLVED || b == UNSOLVED) {
            return UNSOLVED;
        } else {
            assert(a >= 0 && b >= 0);
            return std::max(a, b);
        }
    }

    inline static bool is_less(PolicyCost a, PolicyCost b) {
        assert(a == UNKNOWN || a == UNSOLVED || a >= 0);
        assert(b == UNKNOWN || b == UNSOLVED || b >= 0);
        if (a == UNKNOWN || b == UNKNOWN || a == UNSOLVED || a == b) {
            return false;
        } else if (a >= 0 && b == UNSOLVED) {
            return true;
        } else {
            assert(a >= 0 && b >= 0);
            return a < b;
        }
    }

    explicit Policy(const options::Options &opts);

    /**
     * Executes the policy starting from the given state.
     * If run is complete (i.e., not stopped due to step limit) and the policy solves the state,
     * the actions chosen by the policy until reaching the goal is stored in plan.
     * May throw OutOfResourceException if runtime or memory limit is exceeded.
     * @note uses steps_limit_override with steps_limit field
     **/
    RunResult execute_get_plan(const State &state, std::vector<OperatorID> &plan,
                               std::optional<unsigned int> step_limit_override = std::nullopt);

    /**
     * Works like execute_get_plan but additionally sets path to the sequence of states visited by the policy
     * (this includes the start state and a potential goal state).
     * @note compliant with steps_limit field
     */
    RunResult execute_get_plan_and_path(const State &state, std::vector<OperatorID> &plan,
                                        std::vector<State> &path,
                                        std::optional<unsigned int> step_limit_override = std::nullopt,
                                        bool continue_with_cached_actions = true);

    /**
     * Checks if policy can be executed on path without actually calling the policy,
     * i.e., if there is a complete path using only cached actions
     */
    bool has_complete_cached_path(const State &state);

    /**
     * Executes the policy and returns a path fragment.
     * @note compliant with steps_limit field (execution is aborted if steps_limit is reached)
     * If continue_with_cached_actions is true this means that the policy will no longer be executed but using cached
     * actions is still allowed (so that the path length may be longer considerably longer than the steps limit)
     * step_limit_override can be used to override the steps_limit
     * Returned path cannot be empty
     */
    std::vector<State> execute_get_path_fragment(const State &state,
                                                 std::optional<unsigned int> step_limit_override = std::nullopt,
                                                 bool continue_with_cached_actions = true);

    /**
     * Returns cost of the plan obtained by running the policy on the given state,
     * UNSOLVED if the policy is known to have failed on the state,
     * or UNKNOWN if the evaluation was aborted due to reaching the step limit.
     * A step_limit of 0 (=default) is interpreted as no step limit.
     * Looks up the policy cost cache. If no entry exists, the policy is executed and its resulting cost is cached.
     * Also caches the cost of all intermediate states in the path induced by running the policy.
     * May throw OutOfResourceException if runtime or memory limit is exceeded.
     * @note compliant with steps_limit field
     **/
    PolicyCost compute_policy_cost(const State &state, std::optional<unsigned int> step_limit_override = std::nullopt,
                                   bool continue_with_cached_actions = true);

    /**
     * Returns cost of the resulting plan if successful, or UNSOLVED otherwise.
     * Executes the policy completely.
     * May throw OutOfResourceException if runtime or memory limit is exceeded.
     * @note does not cache the result, meant as a debug function
     * @warning NOT compliant with steps_limit field
     **/
    PolicyCost get_complete_policy_cost(const State &state);

    /**
     * Returns a pair consisting of a lower bound on the policy cost of state and a flag
     * indicating whether the bound is even exact, i.e., the actual policy cost.
     * The cost bound can therefore be Policy::UNSOLVED but never Policy::UNKNOWN.
     * If it is Policy::UNSOLVED, the bound is always exact.
     * Internally calls compute_policy_cost.
     * @note compliant with steps_limit field
     **/
    std::pair<PolicyCost, bool> compute_lower_policy_cost_bound(const State &state,
                                                                std::optional<unsigned int> step_limit_override = std::nullopt);

    /**
     * Similar to compute_lower_policy_cost_bound but does not run policy.
     **/
    std::pair<PolicyCost, bool> read_lower_policy_cost_bound(const State &state);


    /**
     * Returns a pair consisting of an upper bound on the policy cost of state and a flag
     * indicating whether the bound is even exact, i.e., the actual policy cost.
     * The cost bound can therefore be Policy::UNSOLVED but never Policy::UNKNOWN.
     * @note compliant with steps_limit field
     **/
    std::pair<PolicyCost, bool> compute_upper_policy_cost_bound(const State &state) {
        const auto [lower_bound, exact] = compute_lower_policy_cost_bound(state);
        if (exact) {
            return {lower_bound, true};
        } else {
            return {UNSOLVED, false};
        }
    }
    std::pair<PolicyCost, bool> read_upper_policy_cost_bound(const State &state) {
        const auto [lower_bound, exact] = read_lower_policy_cost_bound(state);
        if (exact) {
            return {lower_bound, true};
        } else {
            return {UNSOLVED, false};
        }
    }

    /**
     * Lazy variant of compute_policy_cost, which does not cache the resulting cost and is aborted if
     * max_cost or max_steps is exceeded or if the dead_end_evaluator detects a dead end.
     * @warning in contrast to the base variant, it returns UNSOLVED if the policy is aborted.
     * @warning ignores the steps_limit field of Policy and uses max_steps instead.
     */
    PolicyCost lazy_compute_policy_cost(const State &state,
                                        PolicyCost max_cost, int max_steps,
                                        const std::shared_ptr<Evaluator> &dead_end_evaluator);

    /**
     * Checks if lookup_apply can return the action without calling apply
     */
    bool can_lookup_action(const State &state) const {
        return operator_cache_[state] != NO_CACHED_OPERATOR;
    }

    /**
     * Returns the action stored for the given state in the cache.
     * Must only be called if can_lookup_action(state) returns true.
     **/
    OperatorID lookup_action(const State &state) const;



    void read_running_policy_cache(const std::string &cache_file);
    void set_running_cache_writer(const std::string &cache_file) {
        running_cache_writer = std::make_unique<RunningPolicyCacheWriter>(cache_file);
    }

    /**
     * @brief returns a vector of all cached policy parents of s, i.e.,
     * states in which applying the action the policy chooses results in s.
     */
    const std::vector<StateID> &get_policy_parent_states(StateID s) const {
        return policy_parent[get_state_registry().lookup_state(s)];
    }

    /**
     * @brief returns the cost of the action selected in the given state if the state is cached.
     * @warning assumes that action is cached and is not NO_OPERATOR
     */
    int read_action_cost(const State &s) const;

    /**
     * @brief returns the cost of the action selected in the given state if the state is cached.
     * @warning assumes that action is cached and is not NO_OPERATOR
     */
    int read_action_cost(StateID s) const {
        return read_action_cost(get_state_registry().lookup_state(s));
    }

    /**
     * @brief computes a vector of size path.size() - 1 with the cost of the action chosen by the policy in each state
     * @warning assumes that all action costs can be read, does not execute policy
     */
    std::vector<int> read_path_action_costs(const std::vector<State> &path) const;

    /**
     * Returns true if given state is a goal state.
     */
    [[nodiscard]] bool is_goal(const State &state) const;

    /**
     * Looks up cost of given operator.
     */
    [[nodiscard]] PolicyCost get_operator_cost(OperatorID op) const;

    static void add_options_to_parser(options::OptionParser &);

protected:
    inline static constexpr int NO_CACHED_OPERATOR = -2;

    /**
     * TestingBaseComponent initialization. Overwrite this method to initialize
     * the object once a connection to the environment has been established.
     * Initialization is called prior to the first execute / apply call.
     **/
    void initialize() override {
        TestingBaseComponent::initialize();
    }

    /**
     * Return the action (id) to be applied in the given state.
     * It is the callers responsibility to cache the result in order to guarantee that the policy is never actually
     * executed more than once on a state.
     * apply is called on no state more than once. Should
     * throw OutOfResourceException if runtime or memory limit is exceeded.
     **/
    virtual OperatorID apply(const State &state) = 0;

    /**
     * Set the chosen-action-cache entry of the given state to the given action id.
     **/
    void store_operator(const State &state, const OperatorID &op_id);

private:
    /**
     * Check if state is in cache, if so return the cached action. Otherwise
     * falls back to apply, extending the cache accordingly.
     * Stores new cache entries in running policy cache file if specified.
     * Adds state to successors parent list.
     * May throw OutOfResourceException if runtime or memory limit is exceeded.
     */
    OperatorID lookup_apply(const State &state);

    /**
     * Works like the base version except that the execution is stopped if max_cost is reached or max_steps is exceeded.
     * If max_cost or max_steps is set to a negative value, it is ignored.
     * If a dead_end_evaluator is provided (is not nullptr) the evaluation is stopped if the evaluation
     * returns infinite.
     * @warning ignores the steps_limit field of Policy and uses max_steps instead.
     */
    RunResult execute_get_plan(const State &state, std::vector<OperatorID> &plan, PolicyCost max_cost, int max_steps,
                               const std::shared_ptr<Evaluator> &dead_end_evaluator);


    PerStateInformation<int> operator_cache_;
    PerStateInformation<PolicyCost> policy_cost_cache_;
    using StateVector = std::vector<StateID>;
    // list of parent states for each state s, i.e., parent states in which applying the selected policy leads to s
    // parent vectors must be kept sorted
    PerStateInformation<StateVector> policy_parent;
    std::unique_ptr<RunningPolicyCacheWriter> running_cache_writer;

    // the maximal number of steps to execute the policy; 0 means no limit
    const unsigned int steps_limit;
};

class CachedPolicy : public Policy {
    OperatorID apply(const State &state) override;

public:
    explicit CachedPolicy(const options::Options &opts) : Policy(opts) {}
    static void add_options_to_parser(options::OptionParser &parser) {
        Policy::add_options_to_parser(parser);
    }
};
} // namespace policy_testing
