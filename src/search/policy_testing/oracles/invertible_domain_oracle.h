#pragma once

#include "../oracle.h"

namespace policy_testing {
class InvertibleDomainOracle : public Oracle {
protected:
    TestResult test(Policy &, const State &) override;
public:
    explicit InvertibleDomainOracle(const plugins::Options &opts);
    static void add_options_to_feature(plugins::Feature &feature);

    TestResult test_driver(Policy &policy, const PoolEntry &pool_entry) override;
};
} // namespace policy_testing
