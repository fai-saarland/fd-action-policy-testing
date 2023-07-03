#pragma once

#include <utility>
#include <vector>
#include <memory>
#include <iostream>

#include "../merge_and_shrink/abstraction.h"
#include "../merge_and_shrink/labels.h"
#include "../merge_and_shrink/simulation_relation.h"
#include "../merge_and_shrink/label_relation.h"
#include "../merge_and_shrink/label_relation_noop.h"
#include "../merge_and_shrink/simulation_simple.h"

#include "numeric_simulation_relation.h"
#include "numeric_label_relation.h"

namespace simulations {
class LabelledTransitionSystem;
class Operator;
class SearchProgress;

// This allows us to run experiments with only numeric simulations.  This is very similar
// to using both numeric and boolean simulations but has not been proved to be correct, so
// it is disabled by default.
const static bool only_numeric = false;

const static int MAX_NUM_LABELS = 5000;

class SatisficingDominanceRelation {
    // Auxiliary data-structures to perform successor pruning
    mutable std::set<int> relevant_simulations;
    mutable std::vector<int> parent, parent_ids, succ, succ_ids;
    mutable std::vector<int> values_initial_state_against_parent;

    // Auxiliary data structure to compare against initial state
    std::vector<int> initial_state, initial_state_ids;

    const int truncate_value;
    const int max_simulation_time;
    const int min_simulation_time;
    const int max_total_time;
    const int max_lts_size_to_compute_simulation;

    NumericLabelRelation<int> label_dominance;
    std::unique_ptr<LabelRelation> boolean_label_dominance;
    std::unique_ptr<LabelRelationNoop> boolean_label_dominance_noop;

    std::vector<std::unique_ptr<SimulationRelation>> boolean_simulations;
    std::vector<std::unique_ptr<NumericSimulationRelation<int>>> numeric_simulations;
    mutable std::vector<int> sorted_simulations, unsorted_simulations, candidate_simulations;

    std::vector<int> simulation_of_variable;
    int total_max_value;

    std::unique_ptr<NumericSimulationRelation<int>> init_simulation(Abstraction *_abs);

    std::shared_ptr<TauLabelManager<int>> tau_labels;

    template<typename LTS>
    void compute_ld_simulation_template(std::vector<LTS *> &_ltss,
                                        const LabelMap &labelMap,
                                        bool dump) {
        assert(_ltss.size() == numeric_simulations.size());
        utils::Timer t;


        if (!only_numeric) {
            if (labelMap.get_num_labels() > MAX_NUM_LABELS) {
                boolean_label_dominance_noop->init(_ltss, *this, labelMap);
            } else {
                boolean_label_dominance->init(_ltss, *this, labelMap);
            }


            for (unsigned int i = 0; i < boolean_simulations.size(); i++) {
                if (_ltss[i]->size() > max_lts_size_to_compute_simulation) {
                    std::cout << "Computation of boolean simulation on LTS " << i <<
                        " with " << _ltss[i]->size() << " states cancelled because it is too big."
                              << std::endl;
                    boolean_simulations[i]->cancel_simulation_computation();
                }
            }


            std::cout << "Init LDSim in " << t() << ":" << std::endl;
            do {
                for (unsigned int i = 0; i < boolean_simulations.size(); i++) {
                    if (labelMap.get_num_labels() > MAX_NUM_LABELS) {
                        DominanceRelationSimple<LabelRelationNoop>::
                        update_sim(i, _ltss[i], *boolean_label_dominance_noop, *(boolean_simulations[i]));
                    } else {
                        DominanceRelationSimple<LabelRelation>::
                        update_sim(i, _ltss[i], *boolean_label_dominance, *(boolean_simulations[i]));
                    }
                }

                std::cout << " " << t() << std::endl;
            } while (labelMap.get_num_labels() > MAX_NUM_LABELS ?
                     boolean_label_dominance_noop->update(_ltss, *this) :
                     boolean_label_dominance->update(_ltss, *this));

            std::cout << std::endl << "LDSim computed " << t() << std::endl;
            if (dump) {
                for (unsigned int i = 0; i < _ltss.size(); i++) {
                    //_ltss[i]->dump();
                    boolean_simulations[i]->dump(_ltss[i]->get_names());
                }
            }
        }

        int num_iterations = 0;
        int num_inner_iterations = 0;

        std::cout << "Compute numLDSim on " << _ltss.size() << " LTSs." << std::endl;
        // for (auto lts: _ltss) {
        //    std::cout << lts->size() << " states and " << lts->num_transitions() << " transitions" << std::endl;
        // }

        std::cout << "Compute tau labels" << std::endl;
        tau_labels->initialize(_ltss, labelMap);

        label_dominance.init(_ltss, *this, labelMap);

        for (unsigned int i = 0; i < numeric_simulations.size(); i++) {
            if (_ltss[i]->size() > max_lts_size_to_compute_simulation) {
                std::cout << "Computation of numeric simulation on LTS " << i <<
                    " with " << _ltss[i]->size() << " states cancelled because it is too big." << std::endl;
                numeric_simulations[i]->cancel_simulation_computation(i, _ltss[i]);
            }
        }

        std::vector<int> order_by_size;
        for (unsigned int i = 0; i < numeric_simulations.size(); i++) {
            order_by_size.push_back(i);
        }

        std::sort(order_by_size.begin(), order_by_size.end(), [&](int a, int b) {
                      return _ltss[a]->size() < _ltss[b]->size();
                  });

        std::cout << "Init numLDSim in " << t() << ":" << std::endl;
        bool restart = false;
        do {
            do {
                num_iterations++;
                //label_dominance.dump();
                int remaining_to_compute = order_by_size.size();
                for (int i: order_by_size) {
                    //std::cout << "Updating " << i << " of size " << _ltss[i]->size() << " states and "
                    //          << _ltss[i]->num_transitions() << " transitions" << std::endl;
                    int max_time = std::max(max_simulation_time,
                                            std::min(min_simulation_time,
                                                     1 + max_total_time / remaining_to_compute--));
                    num_inner_iterations += numeric_simulations[i]->update(i, _ltss[i], label_dominance, max_time);
                    //_dominance_relation[i]->dump(_ltss[i]->get_names());
                }
                std::cout << "iteration " << num_iterations << " [" << t() << "]" << std::endl;
            } while (label_dominance.update(_ltss, *this));

            restart = tau_labels->add_noop_dominance_tau_labels(_ltss, label_dominance);
            if (restart) {
                for (int i: order_by_size) {
                    numeric_simulations[i]->init_goal_respecting();
                }
            }
        } while (restart && t() < max_total_time);

        std::cout << std::endl << "Numeric LDSim computed " << t() << std::endl;
        std::cout << "Numeric LDSim outer iterations: " << num_iterations << std::endl;
        std::cout << "Numeric LDSim inner iterations: " << num_inner_iterations << std::endl;

        std::cout << "------" << std::endl;
        for (unsigned int i = 0; i < _ltss.size(); i++) {
            numeric_simulations[i]->statistics();
            std::cout << "------" << std::endl;
        }

        if (dump) {
            std::cout << "------" << std::endl;
            for (unsigned int i = 0; i < _ltss.size(); i++) {
                /*     //_ltss[i]->get_abstraction()->dump_names(); */
                numeric_simulations[i]->dump(_ltss[i]->get_names());
                std::cout << "------" << std::endl;
                label_dominance.dump(_ltss[i], i);
            }
        }
        //label_dominance.dump_equivalent();
        //label_dominance.dump_dominance();
        //}


        total_max_value = 0;
        for (auto &sim: numeric_simulations) {
            total_max_value += sim->compute_max_value();
        }

        remove_candidates(_ltss, label_dominance);
    }


    bool dominates(const std::vector<int> &t_ids,
                   const std::vector<int> &s_ids,
                   const std::set<int> &relevant_simulations, bool strict,
                   bool allow_changing_ordering) const;

    bool dominates(const std::vector<int> &t_ids,
                   const std::vector<int> &s_ids, bool strict, bool allow_changing_ordering) const;

public:

    SatisficingDominanceRelation(Labels *labels,
                                 int truncate_value_,
                                 int max_simulation_time_,
                                 int min_simulation_time_,
                                 int max_total_time_,
                                 int max_lts_size_to_compute_simulation_,
                                 int num_labels_to_use_dominates_in,
                                 std::shared_ptr<TauLabelManager<int>> tau_label_mgr) :
        truncate_value(truncate_value_),
        max_simulation_time(max_simulation_time_),
        min_simulation_time(min_simulation_time_),
        max_total_time(max_total_time_),
        max_lts_size_to_compute_simulation(max_lts_size_to_compute_simulation_),
        label_dominance(labels, num_labels_to_use_dominates_in), tau_labels(std::move(tau_label_mgr)) {
        if (!only_numeric) {
            boolean_label_dominance = std::make_unique<LabelRelation>(labels);
            boolean_label_dominance_noop = std::make_unique<LabelRelationNoop>(labels);
        }
    }


    bool action_selection_pruning(const State &state,
                                  std::vector<OperatorID> &applicable_operators,
                                  /*SearchProgress &search_progress,*/ OperatorCost cost_type) const;

    void prune_dominated_by_parent_or_initial_state(const State &state,
                                                    std::vector<OperatorID> &applicable_operators,
                                                    /*SearchProgress &search_progress,*/ bool parent_ids_stored,
                                                    bool compare_against_parent, bool compare_against_initial_state,
                                                    OperatorCost cost_type) const;

    //Methods to use the dominance relation
    bool pruned_state(const State &state) const;

    void init(const std::vector<Abstraction *> &abstractions);

    void remove_candidates(std::vector<LabelledTransitionSystem *> &_ltss,
                           const NumericLabelRelation<int> &label_dominance);

    void compute_ld_simulation(std::vector<LabelledTransitionSystem *> &_ltss,
                               const LabelMap &labelMap, bool dump) {
        compute_ld_simulation_template(_ltss, labelMap, dump);
    }

    //Methods to access the underlying simulation relations
    const std::vector<std::unique_ptr<NumericSimulationRelation<int>>> &get_simulations() const {
        return numeric_simulations;
    }

    int size() const {
        return numeric_simulations.size();
    }

    NumericSimulationRelation<int> &operator[](int index) {
        return *(numeric_simulations[index]);
    }

    SimulationRelation &get_boolean_simulation(int index) {
        return *(boolean_simulations[index]);
    }

    bool may_simulate(int lts_id, int t_id, int s_id) const {
        return numeric_simulations[lts_id]->may_simulate(t_id, s_id);
    }

    bool positively_simulates(int lts_id, int t_id, int s_id) const {
        if (only_numeric) {
            return numeric_simulations[lts_id]->positively_simulates(t_id, s_id);
        } else {
            return boolean_simulations[lts_id]->simulates(t_id, s_id);
        }
    }

    bool strictly_simulates(int lts_id, int t_id, int s_id) const {
        if (only_numeric) {
            return numeric_simulations[lts_id]->strictly_simulates(t_id, s_id);
        } else {
            return boolean_simulations[lts_id]->strictly_simulates(t_id, s_id);
        }
    }

    const SimulationRelation &get_boolean_simulation(int index) const {
        return *(boolean_simulations[index]);
    }

    const NumericSimulationRelation<int> &operator[](int index) const {
        return *(numeric_simulations[index]);
    }


    bool dominates(const State &t, const State &s, int g_diff) const;


    /* bool strictly_dominates (const State & dominating_state, */
    /*                       const State & dominated_state) const; */

    void set_initial_state(const std::vector<int> &state);

    bool strictly_dominates_initial_state(const State &new_state) const;
};
}
