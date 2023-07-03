#include "variable_order_finder.h"

#include "../utils/utilities.h"
#include "../simulations_manager.h"
#include "../../../task_utils/causal_graph.h"

#include <cassert>
#include <iostream>
#include <vector>

namespace simulations {
VariableOrderFinder::VariableOrderFinder(
    VariableOrderType variable_order_type_, bool is_first)
    : variable_order_type(variable_order_type_) {
    add_init_function([&]() {
                          initialized = true;
                          int var_count = global_simulation_task()->get_num_variables();
                          if (variable_order_type == REVERSE_LEVEL) {
                              for (int i = 0; i < var_count; ++i)
                                  remaining_vars.push_back(i);
                          } else {
                              for (int i = var_count - 1; i >= 0; i--)
                                  remaining_vars.push_back(i);
                          }

                          if (variable_order_type == CG_GOAL_RANDOM || variable_order_type == RANDOM || !is_first)
                              simulations_rng.shuffle(remaining_vars);

                          is_causal_predecessor.resize(var_count, false);
                          is_goal_variable.resize(var_count, false);
                          for (int i = 0; i < global_simulation_task()->get_num_goals(); ++i) {
                              is_goal_variable[global_simulation_task()->get_goal_fact(i).var] = true;
                          }
                      });
}

/*
VariableOrderFinder::VariableOrderFinder(VariableOrderType variable_order_type_, bool is_first,
                                         std::vector<int> remaining_vars_)
    : variable_order_type(variable_order_type_), remaining_vars(std::move(remaining_vars_)) {
    int var_count = global_simulation_task()->get_num_variables();
    std::sort(remaining_vars.begin(), remaining_vars.end());
    if (variable_order_type_ != REVERSE_LEVEL) {
        std::reverse(remaining_vars.begin(), remaining_vars.end());
    }

    if (variable_order_type == CG_GOAL_RANDOM || variable_order_type == RANDOM || !is_first)
        random_shuffle(remaining_vars.begin(), remaining_vars.end());

    is_causal_predecessor.resize(var_count, false);
    is_goal_variable.resize(var_count, false);
    for (int i = 0; i < global_simulation_task()->get_num_goals(); ++i)
        is_goal_variable[global_simulation_task()->get_goal_fact(i).var] = true;

    const auto &causal_graph = global_simulation_task_proxy()->get_causal_graph();
    for (int var_no = 0; var_no < global_simulation_task()->get_num_variables(); ++var_no)
        if (std::find(std::begin(remaining_vars), std::end(remaining_vars), var_no) == std::end(remaining_vars)) {
            selected_vars.push_back(var_no);
            const std::vector<int> &new_vars = causal_graph.get_eff_to_pre(var_no);
            for (int i = 0; i < new_vars.size(); ++i)
                is_causal_predecessor[new_vars[i]] = true;
        }
}*/

void VariableOrderFinder::select_next(int position, int var_no) {
    assert(initialized);
    assert(remaining_vars[position] == var_no);
    remaining_vars.erase(remaining_vars.begin() + position);
    selected_vars.push_back(var_no);
    const auto &causal_graph = global_simulation_task_proxy()->get_causal_graph();
    const std::vector<int> &new_vars = causal_graph.get_eff_to_pre(var_no);
    for (int new_var : new_vars)
        is_causal_predecessor[new_var] = true;
}

bool VariableOrderFinder::done() const {
    assert(initialized);
    return remaining_vars.empty();
}

int VariableOrderFinder::next() {
    assert(initialized);
    assert(!done());
    if (variable_order_type == CG_GOAL_LEVEL || variable_order_type == CG_GOAL_RANDOM) {
        // First run: Try to find a causally connected variable.
        for (int i = 0; i < remaining_vars.size(); ++i) {
            int var_no = remaining_vars[i];
            if (is_causal_predecessor[var_no]) {
                select_next(i, var_no);
                return var_no;
            }
        }
        // Second run: Try to find a goal variable.
        for (int i = 0; i < remaining_vars.size(); ++i) {
            int var_no = remaining_vars[i];
            if (is_goal_variable[var_no]) {
                select_next(i, var_no);
                return var_no;
            }
        }
    } else if (variable_order_type == GOAL_CG_LEVEL) {
        // First run: Try to find a goal variable.
        for (int i = 0; i < remaining_vars.size(); ++i) {
            int var_no = remaining_vars[i];
            if (is_goal_variable[var_no]) {
                select_next(i, var_no);
                return var_no;
            }
        }
        // Second run: Try to find a causally connected variable.
        for (int i = 0; i < remaining_vars.size(); ++i) {
            int var_no = remaining_vars[i];
            if (is_causal_predecessor[var_no]) {
                select_next(i, var_no);
                return var_no;
            }
        }
    } else if (variable_order_type == RANDOM ||
               variable_order_type == LEVEL ||
               variable_order_type == REVERSE_LEVEL) {
        int var_no = remaining_vars[0];
        select_next(0, var_no);
        return var_no;
    }
    std::cerr << "Relevance analysis has not been performed." << std::endl;
    exit_with(EXIT_INPUT_ERROR);
}

void VariableOrderFinder::dump() const {
    std::cout << "Variable order type: ";
    switch (variable_order_type) {
    case CG_GOAL_LEVEL:
        std::cout << "CG/GOAL, tie breaking on level (main)";
        break;
    case CG_GOAL_RANDOM:
        std::cout << "CG/GOAL, tie breaking random";
        break;
    case GOAL_CG_LEVEL:
        std::cout << "GOAL/CG, tie breaking on level";
        break;
    case RANDOM:
        std::cout << "random";
        break;
    case LEVEL:
        std::cout << "by level";
        break;
    case REVERSE_LEVEL:
        std::cout << "by reverse level";
        break;
    default:
        ABORT("Unknown variable order type.");
    }
    std::cout << std::endl;
}
}
