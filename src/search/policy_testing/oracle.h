#pragma once

#include "bug_value.h"
#include "component.h"
#include "policy.h"
#include "pool.h"
#include "utils.h"

#include <memory>

namespace options {
class Options;
}

namespace policy_testing {
class PolicyTestingBaseEngine;

struct TestResult {
    BugValue bug_value;
    PolicyCost upper_cost_bound;

    explicit TestResult(BugValue bug_value, PolicyCost upper_cost_bound) :
        bug_value(bug_value), upper_cost_bound(upper_cost_bound) {}

    explicit TestResult(BugValue bug_value) : bug_value(bug_value), upper_cost_bound(Policy::UNSOLVED) {}

    TestResult() : bug_value(0), upper_cost_bound(Policy::UNSOLVED) {}

    [[nodiscard]] std::string to_string() const {
        return "result\n" + std::to_string((bug_value < UNSOLVED_BUG_VALUE ? bug_value : -1)) + "\n" + std::to_string(
            upper_cost_bound) + "\n";
    }
};

TestResult best_of(const TestResult &left, const TestResult &right) {
    return TestResult(bug_value_best_of(left.bug_value, right.bug_value),
                      Policy::min_cost(left.upper_cost_bound, right.upper_cost_bound));
}

class Oracle : public TestingBaseComponent {
    friend class CompositeOracle;
public:
    // for every reported bug go through all policy parents and report them as bugs as well
    const bool report_parent_bugs;

    // also conduct test for intermediate states
    const bool consider_intermediate_states;

    explicit Oracle(const options::Options &opts);
    virtual void print_statistics() const { }

    /**
     * Remembers the test engine so that it can be used e.g. to report back additional bugs.
     **/
    virtual void set_engine(PolicyTestingBaseEngine *engine);

    /**
     * Check whether the given pool entry is a bug in the policy.
     * The returned TestResult consists of a bug value and an upper cost bound for the pool entry.
     * The bug value should be NOT_APPLICABLE_INDICATOR if the particular method cannot
     * be applied on the given pool entry and UNSOLVED_BUG_VALUE if the given state
     * is solvable but the policy does not induce a plan.
     * Should throw OutOfResourceException if runtime or memory limits are exceeded.
     **/
    virtual TestResult test_driver(Policy &policy, const PoolEntry &pool_entry);

    /** @brief adds a further cost bound for a state to the oracle
     * @warning does not guarantee to flag the state itself as a bug
     * @warning should only be called after test function is called*/
    virtual void add_external_cost_bound(Policy &, const State &, PolicyCost) {}

    /** @brief Goes through all known policy parents (and their policy parents and so on)
     * and reports them as bugs. Increases their bug value if the provided bug value is higher.
     * Stops if the bug_value is not higher than the previously found bug value.
     */
    void report_parents_as_bugs(Policy &policy, const State &state, TestResult test_result);

    /**
     * Debug method to compute the optimal cost of a state.
     * @warning inefficient. Only use in debugging.
     * @warning assumes the policy never changes.
     * @param s the state.
     * @return h*(s). Returns Policy::UNSOLVED if state is dead end.
     */
    [[nodiscard]] int get_optimal_cost(const State &s) const;

    /**
     * Debug method to test is state is indeed a bug of bug_value.
     * @warning runs an optimal planner and is thus very slow. Only use for debugging purposes.
     * @warning expects a bug state with bug_value != 0
     * @note quantitative bugs that are actually quantitative bugs are still accepted
     * @param state the state to test.
     * @param bug_value the claimed bug value.
     * @return true if test is successful.
     */
    [[nodiscard]] bool confirm_bug(const State &state, BugValue bug_value) const;

    virtual void print_debug_info() const {}

protected:
    PolicyTestingBaseEngine *engine_ = nullptr;
    const bool enforce_intermediate;

    /**
     * TestingBaseComponent initialization. Overwrite this method to initialize
     * the object once a connection to the environment has been established.
     * Initialization is called prior to the first test call.
     **/
    void initialize() override {
        TestingBaseComponent::initialize();
    }

    /**
     * Check whether the given pool entry is a bug in the policy.
     * The returned TestResult consists of a bug value and an upper cost bound for the bug candidate.
     * The bug value should be NOT_APPLICABLE_INDICATOR if the particular method cannot
     * be applied on the given pool entry and UNSOLVED_BUG_VALUE if the given state
     * is solvable but the policy does not induce a plan.
     * Should throw OutOfResourceException if runtime or memory limits are exceeded.
     **/
    virtual TestResult test(Policy &policy, const State &bug_candidate) = 0;

    static void add_options_to_parser(options::OptionParser &parser);
};
} // namespace policy_testing
