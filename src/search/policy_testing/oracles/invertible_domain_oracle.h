#pragma once

#include "../oracle.h"

namespace options {
class Options;
class OptionParser;
} // namespace options

namespace policy_testing {
class InvertibleDomainOracle : public Oracle {
protected:
    TestResult test(Policy &, const State &) override;
public:
    explicit InvertibleDomainOracle(const options::Options &opts);
    static void add_options_to_parser(options::OptionParser &parser);

    TestResult test_driver(Policy &policy, const PoolEntry &pool_entry) override;
};
} // namespace policy_testing
