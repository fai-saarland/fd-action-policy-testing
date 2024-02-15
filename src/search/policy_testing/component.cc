#include "component.h"

#include "../state_registry.h"
#include "../task_utils/successor_generator.h"
#include "../task_utils/task_properties.h"
#include "../plugins/plugin.h"

#include <cassert>
#include <limits>

namespace policy_testing {
TestingBaseComponent::TestingBaseComponent(const plugins::Options &opts)
    : debug_(opts.get<bool>("debug")), end_time_(std::numeric_limits<timestamp_t>::max()) {
}

TestingBaseComponent::TestingBaseComponent()
    : debug_(false), end_time_(std::numeric_limits<timestamp_t>::max()) {
}

void
TestingBaseComponent::set_max_time(timestamp_t max_time) {
    for (TestingBaseComponent *c : sub_components_) {
        c->set_max_time(max_time);
    }
    if (max_time < 0) {
        end_time_ = std::numeric_limits<timestamp_t>::max();
    } else {
        end_time_ = get_end_timestamp(max_time);
    }
}

timestamp_t
TestingBaseComponent::get_remaining_time() const {
    return end_time_ - get_timestamp();
}

bool
TestingBaseComponent::are_limits_reached() const {
    return end_time_ <= get_timestamp() || utils::is_out_of_memory();
}

void
TestingBaseComponent::register_sub_component(TestingBaseComponent *component) {
    sub_components_.insert(component);
}

void
TestingBaseComponent::connect_environment(TestingEnvironment *env) {
    assert(env != nullptr);
    assert(env_ == nullptr || env_ == env);
    if (env_ == nullptr) {
        env_ = env;
        for (TestingBaseComponent *c : sub_components_) {
            c->connect_environment(env);
        }
        initialize();
    }
}

TestingEnvironment *
TestingBaseComponent::get_environment() const {
    return env_;
}

std::shared_ptr<AbstractTask>
TestingBaseComponent::get_task() const {
    return env_->get_task();
}

TaskProxy &
TestingBaseComponent::get_task_proxy() const {
    return env_->get_task_proxy();
}

StateRegistry &
TestingBaseComponent::get_state_registry() const {
    return *env_->get_state_registry();
}

successor_generator::SuccessorGenerator &
TestingBaseComponent::get_successor_generator() const {
    return env_->get_successor_generator();
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
    feature.add_option<bool>("debug", "", "false");
}
} // namespace policy_testing
