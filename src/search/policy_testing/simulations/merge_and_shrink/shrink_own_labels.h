#pragma once

#include "shrink_strategy.h"
#include <list>
#include <vector>

namespace simulations {
class ShrinkOwnLabels : public ShrinkStrategy {
    const bool perform_sg_shrinking;
    const bool preserve_optimality;
public:
    explicit ShrinkOwnLabels(const plugins::Options &opts);
    ~ShrinkOwnLabels() override = default;

    [[nodiscard]] std::string name() const override;
    void dump_strategy_specific_options() const override;

    [[nodiscard]] bool reduce_labels_before_shrinking() const override;

    void shrink(Abstraction &abs, int target, bool /* force = false*/) override;
    void shrink_atomic(Abstraction &abs) override;
    void shrink_before_merge(Abstraction &abs1, Abstraction &abs2) override;

    static ShrinkOwnLabels *create_default();
};
}
