#pragma once

#include "component.h"

namespace policy_testing {
class PoolFilter : public TestingBaseComponent {
public:
    virtual void print_statistics() const { }

    /**
     * Return true if the given state should be added to the pool.
     **/
    virtual bool store(const State &) {return true;}

protected:
    /**
     * TestingBaseComponent initialization. Overwrite this method to initialize
     * the object once a connection to the environment has been established.
     * Initialization is called prior to the first store call.
     **/
    void initialize() override {
        TestingBaseComponent::initialize();
    }
};
} // namespace policy_testing
