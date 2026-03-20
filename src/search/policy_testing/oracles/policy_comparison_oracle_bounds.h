#pragma once

#include <optional>
#include "../oracle.h"
#include "../upper_bounds.h"

namespace policy_testing {
class PolicyComparisonOracleBounds : public Oracle {
public:
    explicit PolicyComparisonOracleBounds(const plugins::Options &opts);
    ~PolicyComparisonOracleBounds();

    static void add_options_to_feature(plugins::Feature &feature);

    TestResult test(Policy &policy, const State &state) override;

    void set_upper_bounds(std::shared_ptr<UpperBoundsExtended> upper_bounds);
    std::shared_ptr<UpperBoundsExtended> get_upper_bounds();

    std::shared_ptr<UpperBoundsExtended> upper_bounds;
private:
    bool ignore_quality_bugs;
};
} // namespace policy_testing
