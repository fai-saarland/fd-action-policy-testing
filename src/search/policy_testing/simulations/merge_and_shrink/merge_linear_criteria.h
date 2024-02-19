#pragma once

#include "merge_strategy.h"
#include <vector>
#include <memory>

namespace simulations {
class MergeCriterion;

enum class MergeOrder {
    LEVEL,
    REVERSE_LEVEL,
    RANDOM
};

class MergeLinearCriteria : public MergeStrategy {
    const std::vector<std::shared_ptr<MergeCriterion>> criteria;
    const MergeOrder order;

    //  std::vector<int> selected_vars;
    std::vector<int> remaining_vars;

    void select_next(int var_no);

    //Selects an abstraction based on the criteria
    virtual int next(const std::vector<Abstraction *> &all_abstractions,
                     Abstraction *abstraction = nullptr, int limit_abstract_states_merge = 0,
                     int min_limit_abstract_states_merge = 0,
                     int limit_transitions_merge = 0);

protected:
    void dump_strategy_specific_options() const override;

    void init_strategy(const std::vector<Abstraction *> &abstractions) override;

public:
    explicit MergeLinearCriteria(const plugins::Options &opts);

    ~MergeLinearCriteria() override = default;

    void remove_useless_vars(const std::vector<int> &useless_vars) override;

    std::pair<int, int> get_next(const std::vector<Abstraction *> &all_abstractions,
                                 int limit_abstract_states_merge, int min_limit_abstract_states_merge,
                                 int limit_transitions_merge = 0) override;

    [[nodiscard]] std::string name() const override;

    [[nodiscard]] bool is_linear() const override;
    // virtual bool reduce_labels_before_merge () const;
};
}
