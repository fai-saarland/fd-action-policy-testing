#pragma once

#include "numeric_dominance_oracle.h"
#include <string>

namespace policy_testing {
class UnrelaxationOracle : public NumericDominanceOracle {
protected:

    // maximal number of operation to apply to each state
    unsigned int operations_per_state;

    // maximal number of steps in evaluation of policy in unrelaxed state
    int max_evaluation_steps;

    // evaluator used for dead end detection in policy evaluation of dead end states
    std::shared_ptr<Evaluator> dead_end_eval;

    void initialize() override;

    /**
     * Attempts to unrelax a given state.
     * @param s state to unrelax.
     * @return a vector of pairs consisting of a randomly picked unrelaxed state and the corresponding dominance value.
     * @note if no unrelaxation is possible, the returned vector is empty. If an unrelaxation is possible, the returned
     * vector has up to operations_per_state (mutually different) entries.
     * @note the dominance value may also be negative.
     */
    [[nodiscard]] virtual std::vector<std::pair<State, DominanceValue>> unrelax(const State &s) const;

public:
    explicit UnrelaxationOracle(const plugins::Options &opts);

    static void add_options_to_feature(plugins::Feature &feature);

    TestResult test(Policy &policy, const State &state) override;
};
} // namespace policy_testing
