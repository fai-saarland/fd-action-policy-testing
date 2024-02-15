#pragma once

#include "../task_proxy.h"

#include <memory>
#include <utility>
#include <vector>

class AbstractTask;
class StateRegistry;

namespace successor_generator {
class SuccessorGenerator;
}

namespace policy_testing {
/**
 * General environment shared across the various components of a testing
 * run.
 **/
class TestingEnvironment {
public:
    explicit TestingEnvironment(
        std::shared_ptr<AbstractTask> task,
        StateRegistry *state_registry);

    /**
     * Getters.
     **/
    TaskProxy &get_task_proxy();
    [[nodiscard]] std::shared_ptr<AbstractTask> get_task() const;
    [[nodiscard]] successor_generator::SuccessorGenerator &get_successor_generator() const;
    [[nodiscard]] StateRegistry *get_state_registry() const;


private:
    std::shared_ptr<AbstractTask> task_;
    StateRegistry *state_registry_;
    TaskProxy task_proxy_;
};
} // namespace policy_testing
