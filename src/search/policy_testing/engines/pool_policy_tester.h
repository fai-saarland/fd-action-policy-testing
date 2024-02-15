#pragma once

#include "../pool.h"
#include "testing_base_engine.h"
#include "../novelty_store.h"

namespace policy_testing {
class PoolPolicyTestingEngine : public PolicyTestingBaseEngine {
public:
    explicit PoolPolicyTestingEngine(const plugins::Options &opts);
    static void add_options_to_feature(plugins::Feature &feature);

    void print_statistics() const override;

protected:
    SearchStatus step() override;

private:
    Pool pool_;
    NoveltyStore novelty_store_;

    const unsigned max_steps_;
    const unsigned first_step_;
    const unsigned end_step_;

    unsigned step_;
};
} // namespace policy_testing
