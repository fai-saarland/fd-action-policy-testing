#pragma once

#include "iterative_improvement_oracle.h"
#include "../../evaluator.h"

#include <deque>
#include <iterator>

namespace options {
class Options;

class OptionParser;
} // namespace options

class State;
class Heuristic;

namespace policy_testing {
class SequenceOracle : public Oracle {
    std::shared_ptr<Oracle> first_oracle;
    std::shared_ptr<Oracle> second_oracle;

protected:
    void initialize() override;
    void set_engine(PolicyTestingBaseEngine *engine) override;
    TestResult test(Policy &policy, const State &state) override;

public:
    explicit SequenceOracle(const options::Options &opts);

    static void add_options_to_parser(options::OptionParser &parser);

    BugValue test_driver(Policy &policy, const PoolEntry &entry) override;
};
} // namespace policy_testing
