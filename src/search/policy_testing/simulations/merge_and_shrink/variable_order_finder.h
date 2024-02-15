#pragma once

#include <vector>
#include <string>

namespace simulations {
enum VariableOrderType {
    CG_GOAL_LEVEL,
    CG_GOAL_RANDOM,
    GOAL_CG_LEVEL,
    RANDOM,
    LEVEL,
    REVERSE_LEVEL
};

class VariableOrderFinder {
    const VariableOrderType variable_order_type;

    bool initialized = false;
    // delayed initialization since they depend on the task
    std::vector<int> selected_vars;
    std::vector<int> remaining_vars;
    std::vector<bool> is_goal_variable;
    std::vector<bool> is_causal_predecessor;

    void select_next(int position, int var_no);

public:
    VariableOrderFinder(VariableOrderType variable_order_type_, bool is_first = true);

    /*
    VariableOrderFinder(VariableOrderType variable_order_type_, bool is_first,
                        std::vector<int> remaining_vars_); */

    ~VariableOrderFinder() = default;

    [[nodiscard]] bool done() const;

    int next();

    void dump() const;
};
}
