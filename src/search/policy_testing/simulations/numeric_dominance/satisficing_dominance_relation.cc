#include "satisficing_dominance_relation.h"

#include "numeric_simulation_relation.h"
#include "../merge_and_shrink/abstraction.h"
#include "../merge_and_shrink/labels.h"
#include "../merge_and_shrink/labelled_transition_system.h"
#include "../utils/utilities.h"

#include "../simulations_manager.h"
#include "../../../task_proxy.h"


namespace simulations {
void SatisficingDominanceRelation::init(const std::vector<Abstraction *> &abstractions) {
    numeric_simulations.clear();

    const unsigned int num_variables = global_simulation_task()->get_num_variables();
    simulation_of_variable.resize(num_variables, 0);
    for (auto abs: abstractions) {
        if (!abs) {
            continue;
        }

        int index = numeric_simulations.size();

        if (!only_numeric) {
            boolean_simulations.push_back(std::make_unique<SimulationRelation>(abs));
            boolean_simulations.back()->init_goal_respecting();
        }

        numeric_simulations.push_back(init_simulation(abs));
        for (int v: abs->get_varset()) {
            simulation_of_variable[v] = index;
        }
        candidate_simulations.push_back(index);
        unsorted_simulations.push_back(index);
    }
    parent.resize(num_variables);
    parent_ids.resize(numeric_simulations.size());
    succ.resize(num_variables);
    succ_ids.resize(numeric_simulations.size());
    initial_state.resize(num_variables);
    initial_state_ids.resize(numeric_simulations.size());
    values_initial_state_against_parent.resize(numeric_simulations.size());

    set_initial_state(global_simulation_task()->get_initial_state_values());
}

std::unique_ptr<NumericSimulationRelation<int>>
SatisficingDominanceRelation::init_simulation(Abstraction *_abs) {
    auto res = std::make_unique<NumericSimulationRelation<int>>(_abs, truncate_value, tau_labels);
    res->init_goal_respecting();
    return res;
}

bool SatisficingDominanceRelation::pruned_state(const State &state) const {
    for (auto &sim: numeric_simulations) {
        if (sim->pruned(state)) {
            return true;
        }
    }
    return false;
}

void SatisficingDominanceRelation::remove_candidates(std::vector<LabelledTransitionSystem *> &_ltss,
                                                     const NumericLabelRelation<int> &_label_dominance) {
    if (only_numeric) {
        //cout << "Removing candidates" << endl;
        erase_if(candidate_simulations,
                 [&](int i) {
                     if (!numeric_simulations[i]->has_positive_dominance()) {
                         return true;
                     }
                     // Are any labels that are dangerous for this candidate?
                     auto dangerous_labels = numeric_simulations[i]->get_dangerous_labels(_ltss[i]);

                     erase_if(dangerous_labels,
                              [&](int l) {
                                  return _label_dominance.simulated_by_noop_in_all_other(l, i) &&
                                  _label_dominance.q_dominated_by_noop(l, i) >= 0;
                              });

                     return !dangerous_labels.empty();
                 });
    } else {
        erase_if(candidate_simulations,
                 [&](int i) {
                     if (!boolean_simulations[i]->has_positive_dominance()) {
                         return true;
                     }
                     // Are any labels that are dangerous for this candidate?
                     auto dangerous_labels = boolean_simulations[i]->get_dangerous_labels(_ltss[i]);

                     erase_if(dangerous_labels,
                              [&](int l) {
                                  return _label_dominance.simulated_by_noop_in_all_other(l, i) &&
                                  _label_dominance.q_dominated_by_noop(l, i) >= 0;
                              });

                     return !dangerous_labels.empty();
                 });
    }
    std::cout << "Candidate simulations: " << candidate_simulations.size() << std::endl;
}


bool SatisficingDominanceRelation::dominates(const std::vector<int> &t_ids,
                                             const std::vector<int> &s_ids,
                                             bool strict, bool allow_changing_ordering) const {
    bool strictly_dominates_in_previous_variables = false;
    for (int i: sorted_simulations) {
        int t_id = t_ids[i];
        int s_id = s_ids[i];

        const auto &num_sim = numeric_simulations[i];
        if (!(strictly_dominates_in_previous_variables ?
              num_sim->may_simulate(t_id, s_id) : positively_simulates(i, t_id, s_id))) {
            return false;
        }


        strictly_dominates_in_previous_variables |= strictly_simulates(i, t_id, s_id);
    }

    if (!strict) {
        for (int i: unsorted_simulations) {
            int t_id = t_ids[i];
            int s_id = s_ids[i];

            if (!(strictly_dominates_in_previous_variables ?
                  may_simulate(i, t_id, s_id) : positively_simulates(i, t_id, s_id))) {
                return false;
            }
        }
        return true;
    }


    if (!strictly_dominates_in_previous_variables &&
        candidate_simulations.empty() && !unsorted_simulations.empty()) {
        return false;
    }

    for (int i: unsorted_simulations) {
        int t_id = t_ids[i];
        int s_id = s_ids[i];

        if (!may_simulate(i, t_id, s_id)) {
            return false;
        }
    }
    if (strictly_dominates_in_previous_variables) {
        return true;
    }

    if (!allow_changing_ordering) {
        return false;
    }

    int candidate = -1;
    for (int i: candidate_simulations) {
        if (strictly_simulates(i, t_ids[i], s_ids[i])) {
            candidate = i;
            break;
        }
    }

    if (candidate == -1) {
        return false;
    } else {
        std::cout << candidate << " added to sorted simulations " << std::endl;
        sorted_simulations.push_back(candidate);
        candidate_simulations.erase(find(candidate_simulations.begin(),
                                         candidate_simulations.end(), candidate));

        unsorted_simulations.erase(find(unsorted_simulations.begin(),
                                        unsorted_simulations.end(), candidate));
        return true;
    }
}

bool SatisficingDominanceRelation::strictly_dominates_initial_state(const State &t) const {
    std::vector<int> state_ids(numeric_simulations.size());
    for (int i = 0; i < numeric_simulations.size(); ++i) {
        state_ids[i] = numeric_simulations[i]->get_abstract_state_id(t);
    }
    return dominates(state_ids, initial_state_ids, true, true);
}


bool SatisficingDominanceRelation::dominates(const std::vector<int> &t_ids,
                                             const std::vector<int> &s_ids,
                                             const std::set<int> & /*relevant_simulations*/,
                                             bool strict,
                                             bool allow_changing_ordering) const {
    return dominates(t_ids, s_ids, strict, allow_changing_ordering);
}


bool SatisficingDominanceRelation::action_selection_pruning(const State &state,
                                                            std::vector<OperatorID> &applicable_operators,
                                                            /*SearchProgress & search_progress,*/ OperatorCost /*cost_type*/)
const {
    for (int i = 0; i < global_simulation_task()->get_num_variables(); ++i) {
        parent[i] = state[i].get_value();
    }
    for (int i = 0; i < numeric_simulations.size(); ++i) {
        parent_ids[i] = numeric_simulations[i]->get_abstract_state_id(parent);
    }
    succ = parent;
    auto _succ_ids = parent_ids;
    for (auto op: applicable_operators) {
        for (const auto &prepost: get_preposts(op)) {
            succ[prepost.var] = prepost.post;
            relevant_simulations.insert(simulation_of_variable[prepost.var]);
        }

        bool skip = false;
        for (int sim: relevant_simulations) {
            _succ_ids[sim] = numeric_simulations[sim]->get_abstract_state_id(succ);
            if (_succ_ids[sim] == -1) {
                skip = true;
                break;
            }
        }

        bool apply_action_selection = !skip && dominates(_succ_ids, parent_ids, relevant_simulations, true, false);

        for (int sim: relevant_simulations) {
            _succ_ids[sim] = parent_ids[sim];
        }

        relevant_simulations.clear();

        if (apply_action_selection) {
            // search_progress.inc_action_selection(applicable_operators.size() - 1);
            applicable_operators.clear();
            applicable_operators.push_back(op);
            return true;
        }

        for (const auto &prepost: get_preposts(op)) {
            succ[prepost.var] = parent[prepost.var];
        }
    }

    return false;
}

void SatisficingDominanceRelation::prune_dominated_by_parent_or_initial_state(const State &state,
                                                                              std::vector<OperatorID> &applicable_operators,
                                                                              /*SearchProgress & search_progress,*/ bool parent_ids_stored,
                                                                              bool compare_against_parent,
                                                                              bool compare_against_initial_state,
                                                                              OperatorCost /*cost_type*/) const {
    if (!parent_ids_stored) {
        for (int i = 0; i < global_simulation_task()->get_num_variables(); ++i) {
            succ[i] = state[i].get_value();
        }
        if (compare_against_parent) {
            parent = succ;
            for (int i = 0; i < numeric_simulations.size(); ++i) {
                parent_ids[i] = numeric_simulations[i]->get_abstract_state_id(parent);
            }
        }
    }

    int detected_dead_ends = 0;
    applicable_operators.erase(std::remove_if(applicable_operators.begin(),
                                              applicable_operators.end(),
                                              [&](OperatorID op) {
                                                  for (const auto &prepost: get_preposts(op)) {
                                                      succ[prepost.var] = prepost.post;
                                                      relevant_simulations.insert(
                                                          simulation_of_variable[prepost.var]);
                                                  }

                                                  succ_ids = parent_ids;
                                                  bool proved_prunable = false;
                                                  for (int sim: relevant_simulations) {
                                                      succ_ids[sim] = numeric_simulations[sim]->get_abstract_state_id(
                                                          succ);
                                                      if (succ_ids[sim] == -1) {
                                                          detected_dead_ends++;
                                                          proved_prunable = true;
                                                      }
                                                  }

                                                  if (!proved_prunable && compare_against_parent) {
                                                      proved_prunable = dominates(parent_ids, succ_ids,
                                                                                  relevant_simulations, false,
                                                                                  false);
                                                  }

                                                  relevant_simulations.clear();


                                                  if (!proved_prunable && compare_against_initial_state) {
                                                      proved_prunable = dominates(initial_state_ids, succ_ids,
                                                                                  false, false);
                                                  }

                                                  for (const auto &prepost: get_preposts(op)) {
                                                      succ[prepost.var] = parent[prepost.var];
                                                  }

                                                  return proved_prunable;
                                              }),
                               applicable_operators.end());
}


void SatisficingDominanceRelation::set_initial_state(const std::vector<int> &state) {
    initial_state = state;
    for (int i = 0; i < numeric_simulations.size(); ++i) {
        initial_state_ids[i] = numeric_simulations[i]->get_abstract_state_id(initial_state);
    }
}
}
