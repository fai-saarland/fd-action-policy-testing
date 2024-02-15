#pragma once

#include "shrink_strategy.h"
#include <memory>

namespace simulations {
struct Signature;

class ShrinkComposite : public ShrinkStrategy {
    const std::vector<std::shared_ptr<ShrinkStrategy>> strategies;

public:
    explicit ShrinkComposite(const plugins::Options &opts);
    ShrinkComposite(const plugins::Options &opts, std::vector<std::shared_ptr<ShrinkStrategy>>  sts);

    ~ShrinkComposite() override = default;

    [[nodiscard]] std::string name() const override;
    void dump_strategy_specific_options() const override;

    [[nodiscard]] bool reduce_labels_before_shrinking() const override;

    void shrink(Abstraction &abs, int target, bool force = false) override;
    void shrink_atomic(Abstraction &abs) override;
    void shrink_before_merge(Abstraction &abs1, Abstraction &abs2) override;

    static std::shared_ptr<ShrinkComposite> create_default(const std::vector<std::shared_ptr<ShrinkStrategy>> &sts);
};
}
