#include "testing_environment.h"

#include <utility>

#include "../state_registry.h"
#include "../task_utils/successor_generator.h"

namespace policy_testing {
TestingEnvironment::TestingEnvironment(
    std::shared_ptr<AbstractTask> task,
    StateRegistry *state_registry)
    : task_(std::move(task))
      , state_registry_(state_registry)
      , task_proxy_(*task_) {
}

TaskProxy &
TestingEnvironment::get_task_proxy() {
    return task_proxy_;
}

std::shared_ptr<AbstractTask>
TestingEnvironment::get_task() const {
    return task_;
}

successor_generator::SuccessorGenerator &
TestingEnvironment::get_successor_generator() const {
    return successor_generator::g_successor_generators[task_proxy_];
}

StateRegistry *
TestingEnvironment::get_state_registry() const {
    return state_registry_;
}
} // namespace policy_testing
