#pragma once

#include "../oracle.h"
#include "aras_wrapper.h"

namespace options {
class Options;
class OptionParser;
} // namespace options

namespace policy_testing {
class ArasOracle : public Oracle {
public:
    explicit ArasOracle(const options::Options &opts);
    static void add_options_to_parser(options::OptionParser &parser);

    TestResult test(Policy &policy, const State &state) override;

protected:
    void initialize() override;

private:
    const std::string aras_dir_;
    const int aras_max_time_limit;
    std::unique_ptr<ArasWrapper> aras_;

    const bool cache_results;
    utils::HashMap<StateID, TestResult> result_cache;
};
} // namespace policy_testing
