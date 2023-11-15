#include "task_properties.h"

#include "../utils/memory.h"
#include "../utils/system.h"

#include <algorithm>
#include <iostream>

using namespace std;
using utils::ExitCode;


namespace task_properties {
bool exists_applicable_op(TaskProxy task, const State &state) {
    for (OperatorProxy op : task.get_operators()) {
        if (is_applicable(op, state)) {
            return true;
        }
    }
    return false;
}

vector<FactPair> get_strips_fact_pairs(const AbstractTask *task) {
    std::vector<FactPair> facts;
    for (int var = 0; var < task->get_num_variables(); ++var) {
        for (int val = 0; val < task->get_variable_domain_size(var); ++val) {
            facts.emplace_back(var, val);
            if (!task_properties::is_strips_fact(task, facts.back())) {
                facts.pop_back();
            }
        }
    }
    return facts;
}
bool is_unit_cost(TaskProxy task) {
    for (OperatorProxy op : task.get_operators()) {
        if (op.get_cost() != 1)
            return false;
    }
    return true;
}

bool is_guaranteed_invertible(TaskProxy task) {
    if (has_conditional_effects(task)) {
        return false;
    }
    OperatorsProxy operators = task.get_operators();
    const size_t num_operators = operators.size();
    for (int i = 0; i < num_operators; ++i) {
        OperatorProxy op_i = operators[i];
        const int cost_i = op_i.get_cost();
        PreconditionsProxy pre_i = op_i.get_preconditions();
        std::vector<int> pre_vars_i = pre_i.get_variables();
        EffectsProxy eff_i = op_i.get_effects();
        std::vector<int> eff_vars_i = eff_i.get_variables();
        std::vector<int> prevail_vars_i;
        std::set_difference(pre_vars_i.begin(), pre_vars_i.end(), eff_vars_i.begin(), eff_vars_i.end(),
                            std::inserter(prevail_vars_i, prevail_vars_i.begin()));
        // check if effect variables are subset of precondition variables
        if (!std::includes(pre_vars_i.begin(), pre_vars_i.end(), eff_vars_i.begin(), eff_vars_i.end())) {
            return false;
        }
        bool has_inverse_action = false;
        for (int j = 0; j < num_operators; ++j) {
            if (i == j) {
                continue;
            }
            OperatorProxy op_j = operators[j];
            // test if op_j is inverse operator of op_i
            // check if cost of action and possible inverse action matches
            if (cost_i != op_j.get_cost()) {
                continue;
            }
            // check if vars(eff(op_i)) = vars(eff(op_j))
            EffectsProxy eff_j = op_j.get_effects();
            std::vector<int> eff_vars_j = eff_j.get_variables();
            if (eff_vars_i != eff_vars_j) {
                continue;
            }
            // check for every effect variable v of op_j if we have eff(op_j)[v] = pre(op_i)[v]
            bool matching_pre_and_eff = true;
            for (EffectProxy effect_proxy : eff_j) {
                VariableProxy v = effect_proxy.get_fact().get_variable();
                const int effect_value = effect_proxy.get_fact().get_value();
                const int precondition_value = pre_i.get_condition(v).value().get_value();
                if (effect_value != precondition_value) {
                    matching_pre_and_eff = false;
                    break;
                }
            }
            if (!matching_pre_and_eff) {
                continue;
            }
            // check if op_j is guaranteed to be applicable immediately after op_i has been applied
            bool inverse_applicable = true;
            PreconditionsProxy preconditions_j = op_j.get_preconditions();
            for (FactProxy pre_j : preconditions_j) {
                VariableProxy var = pre_j.get_variable();
                int value = pre_j.get_value();
                if (std::binary_search(eff_vars_i.begin(), eff_vars_i.end(), var.get_id())) {
                    if (eff_i.get_effect(var).value().get_fact().get_value() == value) {
                        continue;
                    }
                }
                if (std::binary_search(prevail_vars_i.begin(), prevail_vars_i.end(), var.get_id())) {
                    if (pre_i.get_condition(var).value().get_value() == value) {
                        continue;
                    }
                }
                inverse_applicable = false;
                break;
            }
            if (!inverse_applicable) {
                continue;
            }
            has_inverse_action = true;
            break;
        }
        if (!has_inverse_action) {
            return false;
        }
    }
    return true;
}

bool has_axioms(TaskProxy task) {
    return !task.get_axioms().empty();
}

void verify_no_axioms(TaskProxy task) {
    if (has_axioms(task)) {
        cerr << "This configuration does not support axioms!"
             << endl << "Terminating." << endl;
        utils::exit_with(ExitCode::SEARCH_UNSUPPORTED);
    }
}


static int get_first_conditional_effects_op_id(TaskProxy task) {
    for (OperatorProxy op : task.get_operators()) {
        for (EffectProxy effect : op.get_effects()) {
            if (!effect.get_conditions().empty())
                return op.get_id();
        }
    }
    return -1;
}

bool has_conditional_effects(TaskProxy task) {
    return get_first_conditional_effects_op_id(task) != -1;
}

void verify_no_conditional_effects(TaskProxy task) {
    int op_id = get_first_conditional_effects_op_id(task);
    if (op_id != -1) {
        OperatorProxy op = task.get_operators()[op_id];
        cerr << "This configuration does not support conditional effects "
             << "(operator " << op.get_name() << ")!" << endl
             << "Terminating." << endl;
        utils::exit_with(ExitCode::SEARCH_UNSUPPORTED);
    }
}

vector<int> get_operator_costs(const TaskProxy &task_proxy) {
    vector<int> costs;
    OperatorsProxy operators = task_proxy.get_operators();
    costs.reserve(operators.size());
    for (OperatorProxy op : operators)
        costs.push_back(op.get_cost());
    return costs;
}


int get_num_facts(const TaskProxy &task_proxy) {
    int num_facts = 0;
    for (VariableProxy var : task_proxy.get_variables())
        num_facts += var.get_domain_size();
    return num_facts;
}

int get_num_total_effects(const TaskProxy &task_proxy) {
    int num_effects = 0;
    for (OperatorProxy op : task_proxy.get_operators())
        num_effects += op.get_effects().size();
    num_effects += task_proxy.get_axioms().size();
    return num_effects;
}

void print_variable_statistics(const TaskProxy &task_proxy) {
    const int_packer::IntPacker &state_packer = g_state_packers[task_proxy];

    int num_facts = 0;
    VariablesProxy variables = task_proxy.get_variables();
    for (VariableProxy var : variables)
        num_facts += var.get_domain_size();

    utils::g_log << "Variables: " << variables.size() << endl;
    utils::g_log << "FactPairs: " << num_facts << endl;
    utils::g_log << "Bytes per state: "
                 << state_packer.get_num_bins() * sizeof(int_packer::IntPacker::Bin)
                 << endl;
}

PerTaskInformation<int_packer::IntPacker> g_state_packers(
    [](const TaskProxy &task_proxy) {
        VariablesProxy variables = task_proxy.get_variables();
        vector<int> variable_ranges;
        variable_ranges.reserve(variables.size());
        for (VariableProxy var : variables) {
            variable_ranges.push_back(var.get_domain_size());
        }
        return utils::make_unique_ptr<int_packer::IntPacker>(variable_ranges);
    }
    );
}
