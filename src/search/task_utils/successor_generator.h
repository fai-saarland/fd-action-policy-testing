#ifndef TASK_UTILS_SUCCESSOR_GENERATOR_H
#define TASK_UTILS_SUCCESSOR_GENERATOR_H

#include "../per_task_information.h"

#include <memory>

class OperatorID;
class State;
class TaskProxy;

namespace operator_generator {
class GeneratorBase;
}
namespace successor_generator {
class SuccessorGenerator {
    std::unique_ptr<operator_generator::GeneratorBase> root;

public:
    explicit SuccessorGenerator(const TaskProxy &task_proxy);
    ~SuccessorGenerator();

    void generate_applicable_ops(const State &state, std::vector<OperatorID> &applicable_ops) const;
    [[nodiscard]] std::vector<OperatorID> generate_applicable_ops(const State &state) const;
};

extern PerTaskInformation<SuccessorGenerator> g_successor_generators;

SuccessorGenerator &get_successor_generator(const TaskProxy &task_proxy);
}

#endif
