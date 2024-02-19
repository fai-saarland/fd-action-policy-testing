#pragma once

#include "merge_strategy.h"

namespace options {
class Options;
}

namespace simulations {
class MergeDFP : public MergeStrategy {
    // border_atomic_composites is the first index at which a composite
    // abstraction can be found in vector of all abstractions as passed
    // as argument to the get_next method.
    bool initialized = false;
    std::size_t border_atomics_composites = 0;
    [[nodiscard]] std::size_t get_corrected_index(int index) const;
protected:
    void dump_strategy_specific_options() const override {}
public:
    MergeDFP();
    ~MergeDFP() override = default;

    void init_strategy(const std::vector <Abstraction * > &abstractions) override;

    // Note: all_abstractions should be a vector of const Abstraction*, but
    // for the moment, compute_label_ranks is a non-const method because it
    // possibly needs to normalize and/or compute distances of some
    // abstractions. This should change when abstractions are always valid.

    // Alvaro: Merge strategies have now a limit on the size of the
    // merge.  If specified (> 0), the pair returned should fit the
    // constraint: a1.size()*a2.size()<=limit
    std::pair<int, int> get_next(const std::vector<Abstraction *> &all_abstractions,
                                 int limit_abstract_states_merge = 0, int min_limit_abstract_states_merge = 0,
                                 int limit_transitions_merge = 0) override;

    [[nodiscard]] std::string name() const override;
    [[nodiscard]] bool is_linear() const override;
};

bool check_valid_size(Abstraction *abstraction, Abstraction *other_abstraction,
                      int limit_abstract_states_merge, int min_limit_abstract_states_merge,
                      int limit_transitions_merge);
}
