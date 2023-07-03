#ifndef TASKS_MODIFIED_INIT_GOALS_TASK_H
#define TASKS_MODIFIED_INIT_GOALS_TASK_H


#include "delegating_task.h"

#include <vector>

namespace extra_tasks {
class ModifiedInitGoalsTask : public tasks::DelegatingTask {
    const std::vector<int> initial_state;
    const std::vector<FactPair> goals;
public:
    ModifiedInitGoalsTask(
        const std::shared_ptr<AbstractTask> &parent,
        const std::vector<int> &&initial_state,
        const std::vector<FactPair> &&goals);
    ~ModifiedInitGoalsTask() override = default;


    int get_num_goals() const override;
    FactPair get_goal_fact(int index) const override;
    std::vector<int> get_initial_state_values() const override;
};
}
#endif
