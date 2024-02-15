#pragma once

#include "iterative_improvement_oracle.h"
#include "../../evaluator.h"

#include <deque>
#include <iterator>

class State;
class Heuristic;

namespace policy_testing {
class CompositeOracle : public Oracle {
    std::shared_ptr<Oracle> qual_oracle;
    std::shared_ptr<Oracle> quant_oracle;
    std::shared_ptr<Oracle> metamorphic_oracle;

    // run external oracle(s) on intermediate states even if pool state could be confirmed as a bug by metamorphic oracle
    bool enforce_external = false;

protected:
    void initialize() override;
    void set_engine(PolicyTestingBaseEngine *engine) override;
    TestResult test(Policy &policy, const State &state) override;

public:
    explicit CompositeOracle(const plugins::Options &opts);

    static void add_options_to_feature(plugins::Feature &feature);

    TestResult test_driver(Policy &policy, const PoolEntry &entry) override;
};
} // namespace policy_testing
