#pragma once

#include "../fuzzing_bias.h"
#include "../policy.h"

namespace policy_testing {
class PlanLengthBias : public PolicyBasedBias {
public:
    explicit PlanLengthBias(const plugins::Options &opts);
    static void add_options_to_feature(plugins::Feature &feature);
    int bias(const State &state, unsigned int budget) override;

    bool can_exclude_state(const State &) override {
        return false;
    }
};
} // namespace policy_testing
