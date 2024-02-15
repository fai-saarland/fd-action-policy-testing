#pragma once

#include "testing_environment.h"
#include "utils.h"

#include <set>

namespace plugins {
class Options;
class Feature;
}

namespace policy_testing {
/**
 * Base class all components of policy testing inherit from. Manages the shared
 * environment, and the initialization of the component once the connection to
 * the environment has been established. Also provides convenience functions to
 * set and check time limits.
 **/
class TestingBaseComponent {
public:
    explicit TestingBaseComponent(const plugins::Options &opts);
    explicit TestingBaseComponent();

    virtual ~TestingBaseComponent() = default;

    /**
     * Sets testing environment and calls initialize(). If called more than
     * once, each sub-sequent call after the first one assume to be given the
     * *same* environment object pointer. Initialization is called just once.
     * This function must be called prior to calling any other function on this
     * object.
     */
    void connect_environment(TestingEnvironment *env);

    /**
     * Retrieve environment.
     **/
    [[nodiscard]] TestingEnvironment *get_environment() const;

    /**
     * Wrappers, calling getters of the environment.
     **/
    [[nodiscard]] std::shared_ptr<AbstractTask> get_task() const;
    [[nodiscard]] TaskProxy &get_task_proxy() const;
    [[nodiscard]] StateRegistry &get_state_registry() const;
    [[nodiscard]] successor_generator::SuccessorGenerator &get_successor_generator() const;

    /** Wrapper function interfacing the state space: **/

    /**
     * Compute the set of applicable operators for the given state.
     **/
    void generate_applicable_ops(
        const State &state,
        std::vector<OperatorID> &applicable_ops) const;

    /**
     * Compute the successor state of the given state and operator.
     **/
    [[nodiscard]] State get_successor_state(const State &state, OperatorID operator_id) const;

    /** Compute and set end timestamp. **/
    void set_max_time(timestamp_t max_time);

    const bool debug_;
    bool initialized = false;

protected:
    /**
     * Initialization. Overwrite this method to initialize the object once
     * a connection to the environment has been established.
     **/
    virtual void initialize() {
        initialized = true;
    }

    /**
     * Register any sub-component. Environment connection will be applied
     * recursively onto all sub-components. Similarly, time limit will propagate
     * to all sub-components. *All* sub-components must have been registered
     * before environment connection has been established.
     **/
    void register_sub_component(TestingBaseComponent *component);

    /** Returns the time left in seconds. **/
    [[nodiscard]] timestamp_t get_remaining_time() const;

    /** Check whether time or memory limits are exhausted. **/
    [[nodiscard]] bool are_limits_reached() const;

    static void add_options_to_feature(plugins::Feature &feature);

private:
    std::set<TestingBaseComponent *> sub_components_;
    TestingEnvironment *env_ = nullptr;
    timestamp_t end_time_;
};
} // namespace policy_testing
