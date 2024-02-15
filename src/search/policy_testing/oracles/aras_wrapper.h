#pragma once

#include "../../abstract_task.h"
#include "../../task_proxy.h"
#include "../plan_file_parser.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace policy_testing {
class ArasWrapper {
public:
    explicit ArasWrapper(
        std::string path,
        std::shared_ptr<AbstractTask> task,
        TaskProxy &task_proxy);
    ~ArasWrapper() = default;

    bool improve_plan(
        int time_limit,
        const State &state,
        std::vector<OperatorID> &plan);

private:
    void
    prepare_aras_input(const State &state, const std::vector<OperatorID> &plan);
    void call_aras(int time_limit);
    bool load(const std::string &file_name, std::vector<OperatorID> &plan);

    [[maybe_unused]] inline static void cleanup();

    std::string aras_directory;
    std::shared_ptr<AbstractTask> task;
    PlanFileParser plan_file_parser;
};
} // namespace policy_testing
