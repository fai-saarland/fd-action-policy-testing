#pragma once

#include "unrelaxation_oracle.h"
#include <string>

namespace policy_testing {
struct AtomicMetamorphicOption {
    int variable;
    int unrelaxed_value;
    int relaxed_value;
    int dominance_value;

    AtomicMetamorphicOption(int variable, int unrelaxed_value, int relaxed_value, int dominance_value) :
        variable(variable),
        unrelaxed_value(unrelaxed_value),
        relaxed_value(relaxed_value),
        dominance_value(dominance_value) {
    }

    AtomicMetamorphicOption() : variable(-1), unrelaxed_value(-1), relaxed_value(-1), dominance_value(-1) {}

    friend std::ostream &operator<<(std::ostream &os, const AtomicMetamorphicOption &p);
};

class AtomicUnrelaxationOracle : public UnrelaxationOracle {
    using AtomicMetamorphicOptions = std::vector<AtomicMetamorphicOption>;
    // possible (un)relaxations for each variable and value
    std::vector<std::vector<AtomicMetamorphicOptions>> possible_relaxations;
    std::vector<std::vector<AtomicMetamorphicOptions>> possible_unrelaxations;

    /**
     * @brief Relaxes given state (@param s) wrt the metamorphic operator (@param o) and @returns the relaxed state.
     */
    [[nodiscard]] State relax(const State &s, const AtomicMetamorphicOption &o) const;

    /**
     * @brief Unrelaxes given state (@param s) wrt the metamorphic operator (@param o) and @returns the unrelaxed state.
     */
    [[nodiscard]] State unrelax(const State &s, const AtomicMetamorphicOption &o) const;

    [[nodiscard]] unsigned int num_possible_relaxations(const State &s) const;
    [[nodiscard]] unsigned int num_possible_unrelaxations(const State &s) const;

    [[nodiscard]] AtomicMetamorphicOption get_relaxation(const State &s, unsigned int index) const;
    [[nodiscard]] AtomicMetamorphicOption get_unrelaxation(const State &s, unsigned int index) const;


    /**
     * @brief @returns an unsorted vector consisting of @param n elements of range [0, @param range)
     * @note if n > range, assume n = range
     */
    static std::vector<unsigned int> pick_n_of_range(unsigned int n, unsigned int range);

    /**
     * Attempts to relax a given state.
     * @param s state to relax.
     * @return a vector of pairs consisting of a randomly picked relaxed state and the corresponding dominance value.
     * @note if no relaxation is possible, the returned vector is empty. If a relaxation is possible, the returned
     * vector has up to @field operations_per_state (mutually different) entries.
     * @note the dominance value may also be negative.
     */
    [[nodiscard]] std::vector<std::pair<State, DominanceValue>> relax(const State &s) const;

    /**
     * Attempts to unrelax a given state.
     * @param s state to unrelax.
     * @return a vector of pairs consisting of a randomly picked unrelaxed state and the corresponding dominance value.
     * @note if no unrelaxation is possible, the returned vector is empty. If an unrelaxation is possible, the returned
     * vector has up to @field operations_per_state (mutually different) entries.
     * @note the dominance value may also be negative.
     */
    [[nodiscard]] std::vector<std::pair<State, DominanceValue>> unrelax(const State &s) const override;

protected:
    void initialize() override;

public:
    explicit AtomicUnrelaxationOracle(const plugins::Options &opts);

    static void add_options_to_feature(plugins::Feature &feature);

    void print_debug_info() const override;
};
} // namespace policy_testing
