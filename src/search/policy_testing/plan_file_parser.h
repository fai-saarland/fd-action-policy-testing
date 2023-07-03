#pragma once

#include "../operator_id.h"
#include "../task_proxy.h"

#include <iostream>
#include <string>
#include <unordered_map>

namespace policy_testing {
class PlanFileParser {
public:
    explicit PlanFileParser(TaskProxy &task_proxy);
    ~PlanFileParser() = default;

    bool parse(const std::string &path, std::vector<OperatorID> &plan) const;
    bool parse(std::istream &in, std::vector<OperatorID> &plan) const;

private:
    std::unordered_map<std::string, int> reverse_mapping_;
};
} // namespace policy_testing
