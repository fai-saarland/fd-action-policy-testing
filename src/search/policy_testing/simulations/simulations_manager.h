#pragma once

#include <utility>
#include <set>

#include "../../task_proxy.h"
#include "../../task_utils/task_properties.h"
#include "../../operator_cost.h"
#include "../../utils/rng.h"

namespace simulations {
[[nodiscard]] inline static const AbstractTask *global_simulation_task();

[[nodiscard]] inline static const TaskProxy *global_simulation_task_proxy();

struct Prevail {
    int var;
    int prev;

    Prevail(int v, int p) : var(v), prev(p) {}

    [[nodiscard]] bool is_applicable(const State &state) const {
        assert(var >= 0 && var < global_simulation_task()->get_num_variables());
        assert(prev >= 0 && prev < global_simulation_task()->get_variable_domain_size(var));
        return state[var].get_value() == prev;
    }

    bool operator==(const Prevail &other) const {
        return var == other.var && prev == other.prev;
    }

    bool operator!=(const Prevail &other) const {
        return !(*this == other);
    }

    void dump() const;
};

struct PrePost {
    int var;
    int pre, post;
    std::vector<Prevail> cond;

    PrePost(int v, int pr, int po, std::vector<Prevail> co) : var(v), pre(pr), post(po), cond(std::move(co)) {}

    [[nodiscard]] bool is_applicable(const State &state) const {
        assert(var >= 0 && var < global_simulation_task()->get_num_variables());
        assert(pre == -1 || (pre >= 0 && pre < global_simulation_task()->get_variable_domain_size(var)));
        return pre == -1 || state[var].get_value() == pre;
    }

    void dump() const;
};

struct SimulationsManager {
    /**
     * Global variables
     */
    inline static std::shared_ptr<AbstractTask> task = nullptr;
    inline static std::unique_ptr<TaskProxy> task_proxy = nullptr;
    inline static int num_operators = -1;
    inline static int num_variables = -1;
    inline static bool unit_cost = -1;
    inline static bool conditional_effects = -1;
    inline static std::vector<std::vector<Prevail>> operator_prevails;
    inline static std::vector<std::vector<PrePost>> operator_preposts;
    // simulate hack used in merge and shrink
    inline static std::vector<bool> is_dead_operator;
    // simulate hack used in sym abstraction
    inline static std::vector<bool> op_marker_1;
    inline static std::vector<bool> op_marker_2;

    inline static bool initialized = false;
    inline static std::vector<std::function<void(void)>> initialization_functions;

    inline static void set_simulation_task(std::shared_ptr<AbstractTask> task_) {
        initialized = true;
        task = std::move(task_);
        task_proxy = std::make_unique<TaskProxy>(*task);
        unit_cost = task_properties::is_unit_cost(*task_proxy);
        conditional_effects = task_properties::has_conditional_effects(*task_proxy);
        num_variables = task->get_num_variables();
        num_operators = task->get_num_operators();
        operator_prevails.clear();
        operator_prevails.reserve(num_operators);
        operator_preposts.clear();
        operator_preposts.reserve(num_operators);
        is_dead_operator = std::vector<bool>(num_operators, false);
        op_marker_1 = std::vector<bool>(num_operators, false);
        op_marker_2 = std::vector<bool>(num_operators, false);

        // compute prevails and preposts
        for (const auto &op: task_proxy->get_operators()) {
            const auto &preconditions = op.get_preconditions();
            const auto &effects = op.get_effects();

            // collect preconditions and effects on each variable
            std::vector<int> precondition_on_var(num_variables, -1);
            std::vector<int> effect_on_var(num_variables, -1);
            for (const auto &pre: preconditions) {
                const int var = pre.get_variable().get_id();
                const int val = pre.get_value();
                assert((precondition_on_var[var] == -1 || precondition_on_var[var] == val) &&
                       "Conflicting preconditions for variable");
                precondition_on_var[var] = val;
            }
            for (const auto &eff: effects) {
                const int var = eff.get_fact().get_variable().get_id();
                const int val = eff.get_fact().get_value();
                effect_on_var[var] = val;
            }

            std::vector<Prevail> prevails;
            std::vector<PrePost> preposts;

            // translate effects to PrePosts or Prevails
            for (const auto &eff: effects) {
                const int var = eff.get_fact().get_variable().get_id();
                const int eff_val = eff.get_fact().get_value();
                const int pre_val = precondition_on_var[var];
                const auto &fire_conditions = eff.get_conditions();
                // TODO what about fire condition on the effect_variable
                //  (can this be interpreted as a Prevail if effect changes its value?)
                std::vector<Prevail> translated_fire_conditions;
                for (const auto &fire_cond: fire_conditions) {
                    translated_fire_conditions.emplace_back(fire_cond.get_variable().get_id(),
                                                            fire_cond.get_value());
                }
                if (fire_conditions.empty() && pre_val == eff_val) {
                    // model effect as a Prevail
                    assert(pre_val == eff_val);
                    prevails.emplace_back(var, pre_val);
                } else {
                    // model effect as a prepost
                    preposts.emplace_back(var, pre_val, eff_val, std::move(translated_fire_conditions));
                }
            }

            // add remaining Prevail conditions
            for (int var = 0; var < num_variables; ++var) {
                const int pre_val = precondition_on_var[var];
                if ((pre_val == -1) || effect_on_var[var] != -1) {
                    // no need to add a Prevail if there is no precondition on that variable or if there is
                    // an effect on that variable (in which case the variable has been dealt with above)
                    continue;
                }
                prevails.emplace_back(var, pre_val);
            }
            operator_prevails.push_back(std::move(prevails));
            operator_preposts.push_back(std::move(preposts));
        }
        for (const auto &init_function : initialization_functions) {
            init_function();
        }
    }
};

inline static void add_init_function(const std::function<void(void)> &init_function) {
    SimulationsManager::initialization_functions.push_back(init_function);
}


[[nodiscard]] inline static const AbstractTask *global_simulation_task() {
    assert(SimulationsManager::initialized);
    assert(SimulationsManager::task);
    return SimulationsManager::task.get();
}

[[nodiscard]] inline static const TaskProxy *global_simulation_task_proxy() {
    assert(SimulationsManager::initialized);
    assert(SimulationsManager::task_proxy);
    return SimulationsManager::task_proxy.get();
}

[[nodiscard]] inline static const std::vector<Prevail> &get_prevails(int op) {
    assert(SimulationsManager::initialized);
    return SimulationsManager::operator_prevails[op];
}

[[nodiscard]] inline static const std::vector<PrePost> &get_preposts(int op) {
    assert(SimulationsManager::initialized);
    return SimulationsManager::operator_preposts[op];
}

[[nodiscard]] inline static const std::vector<Prevail> &get_prevails(OperatorID op) {
    assert(SimulationsManager::initialized);
    return get_prevails(op.get_index());
}

[[nodiscard]] inline static const std::vector<PrePost> &get_preposts(OperatorID op) {
    assert(SimulationsManager::initialized);
    return get_preposts(op.get_index());
}

[[nodiscard]] inline static OperatorProxy get_op_proxy(int op) {
    assert(SimulationsManager::initialized);
    return global_simulation_task_proxy()->get_operators()[op];
}

[[nodiscard]] inline static OperatorProxy get_op_proxy(OperatorID op) {
    assert(SimulationsManager::initialized);
    return get_op_proxy(op.get_index());
}

inline static void set_dead(int op) {
    assert(SimulationsManager::initialized);
    SimulationsManager::is_dead_operator[op] = true;
}

inline static void set_dead(OperatorID op) {
    assert(SimulationsManager::initialized);
    set_dead(op.get_index());
}

inline static bool is_dead(int op) {
    assert(SimulationsManager::initialized);
    return SimulationsManager::is_dead_operator[op];
}

inline static bool is_dead(OperatorID op) {
    assert(SimulationsManager::initialized);
    return is_dead(op.get_index());
}

inline static void set_marker_1(int op, bool value) {
    assert(SimulationsManager::initialized);
    SimulationsManager::op_marker_1[op] = value;
}

inline static void set_marker_2(int op, bool value) {
    assert(SimulationsManager::initialized);
    SimulationsManager::op_marker_2[op] = value;
}

inline static bool get_marker_1(int op) {
    assert(SimulationsManager::initialized);
    return SimulationsManager::op_marker_1[op];
}

inline static bool get_marker_2(int op) {
    assert(SimulationsManager::initialized);
    return SimulationsManager::op_marker_2[op];
}

inline static bool has_unit_cost() {
    assert(SimulationsManager::initialized);
    return SimulationsManager::unit_cost;
}

inline static bool has_conditional_effects() {
    assert(SimulationsManager::initialized);
    return SimulationsManager::conditional_effects;
}

inline static bool is_unit_cost_task(const OperatorCost &cost_type) {
    assert(SimulationsManager::initialized);
    bool is_unit_cost = true;
    for (const auto &op : global_simulation_task_proxy()->get_operators()) {
        if (get_adjusted_action_cost(op, cost_type, has_unit_cost()) != 1) {
            is_unit_cost = false;
            break;
        }
    }
    return is_unit_cost;
}

inline static void get_vars(int op_id, std::set<int> &pre_vars, std::set<int> &eff_vars) {
    assert(SimulationsManager::initialized);
    for (auto &p: SimulationsManager::operator_prevails[op_id]) {
        pre_vars.insert(p.var);
    }
    for (auto &p: SimulationsManager::operator_preposts[op_id]) {
        eff_vars.insert(p.var);
        if (p.pre != -1) {
            pre_vars.insert(p.var);
        }
    }
}

inline static utils::RandomNumberGenerator simulations_rng = utils::RandomNumberGenerator(2022);
} // simulations
