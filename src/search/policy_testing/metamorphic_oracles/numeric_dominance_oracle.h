#pragma once

#include "../oracle.h"
#include "../simulations/numeric_dominance/numeric_dominance_relation.h"

namespace options {
class Options;

class OptionParser;
} // namespace options

namespace simulations {
class Abstraction;
class AbstractionBuilder;

template<typename T>
class TauLabelManager;

class LDSimulation;

template<typename T>
class NumericDominanceRelation;
}

namespace policy_testing {
class NumericDominanceOracle : public Oracle {
    friend class CompositeOracle;
    friend class DominanceBias;
    std::shared_ptr<simulations::AbstractionBuilder> abstraction_builder;
    std::vector<std::unique_ptr<simulations::Abstraction>> abstractions;
    std::shared_ptr<simulations::TauLabelManager<int>> tau_labels;
    std::unique_ptr<simulations::LDSimulation> ldSim;
    const int truncate_value;
    const int max_simulation_time;
    const int min_simulation_time;
    const int max_total_time;
    const int max_lts_size_to_compute_simulation;
    const int num_labels_to_use_dominates_in;
    bool dump;

    std::string sim_file;
    bool write_sim_and_exit;
    bool test_serialization;

    enum class LocalBugTest {NONE, ONE, ALL};
    inline static const std::vector<std::string> local_bug_test_strings {"NONE", "ONE", "ALL"};

    const LocalBugTest local_bug_test_kind;

    /**
     * Checks if choosing operator op in state s is a bug under the local bug criterion.
     * @param policy The tested policy.
     * @param s The original state.
     */
    BugValue local_bug_test_first(Policy &policy, const State &s);

    /**
     * Checks if choosing operator op in state s is a bug under the local bug criterion.
     * @param policy The tested policy.
     * @param s The original state.
     * @param op The operator chosen by the policy in s.
     * @param t The successor state resulting from applying op in s.
     * @param additional_bug_value bug value that can be safely added to the inferred local bug value for s
     *  (must not be negative, if positive state is always reported as bug)
     * @return the inferred local bug value for this step + additional_bug_value
     * @note choose this variant if the successor is already known.
     * @warning assumes that the policy solved s, i.e., policy.compute_policy_cost(s) != Policy::UNSOLVED.
     */
    BugValue
    local_bug_test_step(Policy &policy, const State &s, OperatorID op, const State &t,
                        BugValue additional_bug_value = 0);

    /**
     * Apply local bug test to entire execution sequence.
     * @param s start state
     * @return bug value of start state s
     */
    BugValue complete_local_bug_test(Policy &policy, const State &s);

protected:
    using DominanceValue = int;
    std::unique_ptr<simulations::NumericDominanceRelation<DominanceValue>> numeric_dominance_relation;
    std::unique_ptr<simulations::StrippedNumericDominanceRelation> stripped_numeric_dominance_relation;

    // lower bound for the lowest negative but finite dominance value
    int minimal_finite_dominance_value = 0;

    bool read_simulation;

    void initialize() override;

    /**
     * Debug method to confirm dominance value.
     * @warning inefficient. Only use for debugging purposes.
     * @param dominated_state
     * @param dominating_state
     * @param dominance_value
     * @return true iff D(dominated_state, dominating_state) <= h*(dominated_state) - h*(dominating_state);
     */
    [[maybe_unused]] [[nodiscard]] bool confirm_dominance_value(
        const State &dominated_state, const State &dominating_state, int dominance_value) const;

    /**
     * @brief Returns the computed dominance value in the order used in the paper.
     * @note order of states is switched compared to q_dominates_value
     */
    [[nodiscard]] int D(const State &state0, const State &state1) const {
        if (read_simulation) {
            assert(stripped_numeric_dominance_relation);
            return stripped_numeric_dominance_relation->q_dominates_value(state1, state0);
        } else {
            assert(numeric_dominance_relation);
            return numeric_dominance_relation->q_dominates_value(state1, state0);
        }
    }

    /**
     * @brief Returns the computed dominance value in the order used in the paper.
     * @note order of states is switched compared to q_dominates_value
     */
    [[nodiscard]] int D(const std::vector<int> &state0, const std::vector<int> &state1) const {
        if (read_simulation) {
            assert(stripped_numeric_dominance_relation);
            return stripped_numeric_dominance_relation->q_dominates_value(state1, state0);
        } else {
            assert(numeric_dominance_relation);
            return numeric_dominance_relation->q_dominates_value(state1, state0);
        }
    }

public:

    /**
     * Performs a local bug test (if not disabled) either only one step or completely (depending on the configuration).
     * @param policy The tested policy.
     * @param s The start state.
     * @return the inferred bug value for s. Returns 0 if no test is executed.
     */
    BugValue local_bug_test(Policy &policy, const State &s);

    bool could_be_based_on_atomic_abstraction();

    explicit NumericDominanceOracle(const options::Options &opts);

    static void add_options_to_parser(options::OptionParser &parser);

    TestResult test(Policy &policy, const State &state) override;
};
} // namespace policy_testing
