#pragma once

#include "../pool.h"
#include "../utils/novelty_store.h"
#include "testing_base_engine.h"

namespace policy_testing {
class PoolPolicyTestingEngine : public PolicyTestingBaseEngine {
public:
    explicit PoolPolicyTestingEngine(const plugins::Options &opts);
    static void add_options_to_feature(plugins::Feature &feature);

    void print_statistics() const override;

protected:
    SearchStatus step() override;

private:
    Pool pool;
    NoveltyStore novelty_store;

    const unsigned max_steps;
    const unsigned first_step;
    const unsigned end_step;

    unsigned int pool_step;
};
} // namespace policy_testing
