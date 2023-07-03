#pragma once

#include "../fuzzing_bias.h"
#include "../policy.h"

namespace options {
class Options;
class OptionParser;
} // namespace options

namespace policy_testing {
class PlanLengthBias : public PolicyBasedBias {
public:
    explicit PlanLengthBias(const options::Options &opts);
    static void add_options_to_parser(options::OptionParser &parser);
    int bias(const State &state, unsigned int budget) override;

    bool can_exclude_state(const State &) override {
        return false;
    }
};
} // namespace policy_testing
