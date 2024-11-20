#include "component.h"

#include "../state_registry.h"
#include "../task_utils/successor_generator.h"
#include "../task_utils/task_properties.h"
#include "../plugins/plugin.h"

#include <cassert>
#include <limits>

namespace policy_testing {
TestingBaseComponent::TestingBaseComponent(const plugins::Options &opts)
    : debug(opts.get<bool>("debug")),
      end_time(std::numeric_limits<timestamp_t>::max()) {
}

TestingBaseComponent::TestingBaseComponent()
    : debug(false), end_time(std::numeric_limits<timestamp_t>::max()) {
}

void
TestingBaseComponent::set_max_time(timestamp_t max_time) {
    for (TestingBaseComponent *c : sub_components) {
        c->set_max_time(max_time);
    }
    if (max_time < 0) {
        end_time = std::numeric_limits<timestamp_t>::max();
    } else {
        end_time = get_end_timestamp(max_time);
    }
}

timestamp_t
TestingBaseComponent::get_remaining_time() const {
    return end_time - get_timestamp();
}

bool
TestingBaseComponent::are_limits_reached() const {
    return end_time <= get_timestamp() || utils::is_out_of_memory();
}

void
TestingBaseComponent::register_sub_component(TestingBaseComponent *component) {
    sub_components.insert(component);
}

void
TestingBaseComponent::connect_environment(TestingEnvironment *env) {
    assert(env != nullptr);
    assert(component_env == nullptr || component_env == env);
    if (component_env == nullptr) {
        component_env = env;
        for (TestingBaseComponent *c : sub_components) {
            c->connect_environment(env);
        }
        initialize();
    }
}

TestingEnvironment *
TestingBaseComponent::get_environment() const {
    return component_env;
}

std::shared_ptr<AbstractTask>
TestingBaseComponent::get_task() const {
    assert(component_env);
    return component_env->get_task();
}

TaskProxy &
TestingBaseComponent::get_task_proxy() const {
    assert(component_env);
    return component_env->get_task_proxy();
}

StateRegistry &
TestingBaseComponent::get_state_registry() const {
    assert(component_env);
    return *component_env->get_state_registry();
}

successor_generator::SuccessorGenerator &
TestingBaseComponent::get_successor_generator() const {
    assert(component_env);
    return component_env->get_successor_generator();
}

void
TestingBaseComponent::generate_applicable_ops(
    const State &state,
    std::vector<OperatorID> &applicable_ops) const {
    get_successor_generator().generate_applicable_ops(state, applicable_ops);
}

State
TestingBaseComponent::get_successor_state(
    const State &state,
    OperatorID operator_id) const {
    assert(state.get_registry() == &get_state_registry());
    assert(task_properties::is_applicable(
               get_task_proxy().get_operators()[operator_id], state));
    return get_state_registry().get_successor_state(
        state, get_task_proxy().get_operators()[operator_id]);
}

void
TestingBaseComponent::add_options_to_feature(plugins::Feature &feature) {
    feature.add_option<bool>("debug", "Run in (very costly) debug mode.", "false");
}
} // namespace policy_testing
