#pragma once

#include "component.h"
#include "pool.h"

namespace policy_testing {
class PlanCostEstimator : public TestingBaseComponent {
public:
    enum ReturnCode {
        UNKNOWN = -2,
        DEAD_END = -1,
    };

    /**
     * Compute plan cost estimate for given state. Returns DEAD_END if state is
     * a dead end. May return UNKNOWN if plan cost cannot be estimated. Should
     * throw OutOfResourceException if runtime or memory limits are exceeded.
     **/
    virtual int compute_value(const State &state) = 0;

protected:
    /**
     * TestingBaseComponent initialization. Overwrite this method to initialize
     * the object once a connection to the environment has been established.
     * Initialization is called prior to the first compute_value call.
     **/
    void initialize() override {
        TestingBaseComponent::initialize();
    }
};
} // namespace policy_testing
