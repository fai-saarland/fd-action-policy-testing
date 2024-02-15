#include "modified_init_goals_task.h"


namespace extra_tasks {
ModifiedInitGoalsTask::ModifiedInitGoalsTask(
    const std::shared_ptr<AbstractTask> &parent,
    const std::vector<int> &&initial_state,
    const std::vector<FactPair> &&goals)
    : DelegatingTask(parent),
      initial_state(initial_state),
      goals(goals) {
}

int ModifiedInitGoalsTask::get_num_goals() const {
    return goals.size();
}

FactPair ModifiedInitGoalsTask::get_goal_fact(int index) const {
    return goals[index];
}

std::vector<int> ModifiedInitGoalsTask::get_initial_state_values() const {
    return initial_state;
}
}
