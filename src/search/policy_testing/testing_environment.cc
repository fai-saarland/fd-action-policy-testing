#include "testing_environment.h"

#include <utility>

#include "../state_registry.h"
#include "../task_utils/successor_generator.h"

namespace policy_testing {
TestingEnvironment::TestingEnvironment(std::shared_ptr<AbstractTask> t, StateRegistry *state_registry)
    : task(std::move(t)), state_registry(state_registry), task_proxy(*task) {
}

TaskProxy &
TestingEnvironment::get_task_proxy() {
    return task_proxy;
}

std::shared_ptr<AbstractTask>
TestingEnvironment::get_task() const {
    return task;
}

successor_generator::SuccessorGenerator &
TestingEnvironment::get_successor_generator() const {
    return successor_generator::g_successor_generators[task_proxy];
}

StateRegistry *
TestingEnvironment::get_state_registry() const {
    return state_registry;
}
} // namespace policy_testing
