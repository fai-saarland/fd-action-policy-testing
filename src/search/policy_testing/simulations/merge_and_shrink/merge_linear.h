#pragma once

#include "merge_strategy.h"

#include "variable_order_finder.h"

namespace options {
class Options;
}

namespace simulations {
//Alvaro: Merge linear will behave as a non-linear merge in case that
// limit_abstract_states_merge is set
class MergeLinear : public MergeStrategy {
    VariableOrderFinder order;
    bool need_first_index;
protected:
    void dump_strategy_specific_options() const override;
public:
    explicit MergeLinear(const options::Options &opts);
    ~MergeLinear() override = default;

    void init_strategy(const std::vector <Abstraction * > &) override {}


    // Alvaro: Merge strategies have now a limit on the size of the
    // merge.  If specified (> 0), the pair returned should fit the
    // constraint: a1.size()*a2.size()<=limit
    std::pair<int, int> get_next(const std::vector<Abstraction *> &all_abstractions,
                                 int limit_abstract_states_merge = 0, int min_limit_abstract_states_merge = 0,
                                 int limit_transitions_merge = 0) override;
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] bool is_linear() const override;
};
}
