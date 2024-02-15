#include "ld_simulation.h"

#include "abstraction.h"
#include "shrink_bisimulation.h"
#include "simulation_simple.h"
#include "simulation_identity.h"
#include "merge_strategy.h"
#include "labelled_transition_system.h"
#include "opt_order.h"
#include "label_reducer.h"
#include <boost/dynamic_bitset.hpp>
#include "label_relation.h"
#include "label_relation_identity.h"
#include "label_relation_noop.h"
#include "alternative_label_relation.h"
#include "../utils/debug.h"
#include "../numeric_dominance/numeric_dominance_relation.h"
#include "../../../task_utils/causal_graph.h"
#include "../../../plugins/plugin.h"

namespace simulations {
std::ostream &operator<<(std::ostream &os, const LabelDominanceType &type) {
    switch (type) {
    case LabelDominanceType::NONE:
        return os << "none";
    case LabelDominanceType::NOOP:
        return os << "noop";
    case LabelDominanceType::NORMAL:
        return os << "normal";
    case LabelDominanceType::ALTERNATIVE:
        return os << "alternative";
    default:
        std::cerr << "Name of LabelDominanceType not known";
        exit(-1);
    }
}

std::ostream &operator<<(std::ostream &os, const SimulationType &type) {
    switch (type) {
    case SimulationType::NONE:
        return os << "none";
    case SimulationType::SIMPLE:
        return os << "simple";
    // case SimulationType::COMPLEX:
    //    return os << "complex";
    default:
        std::cerr << "Name of SimulationType not known";
        exit(-1);
    }
}

LDSimulation::LDSimulation(bool unit_cost, const plugins::Options &opts, OperatorCost cost_type) :
    labels(new Labels(unit_cost, opts, cost_type)) {
}

LDSimulation::~LDSimulation() {
    //Abstractions should use shared_ptr to avoid a leak here
    for (auto abs: abstractions) {
        delete abs;
    }
}

std::unique_ptr<DominanceRelation> LDSimulation::create_dominance_relation(SimulationType simulation_type,
                                                                           LabelDominanceType label_dominance_type,
                                                                           int switch_off_label_dominance) {
    switch (simulation_type) {
    case SimulationType::NONE:
        return std::unique_ptr<DominanceRelation>(
            new DominanceRelationIdentity<LabelRelationIdentity>(labels.get()));
    case SimulationType::SIMPLE:
        switch (label_dominance_type) {
        case LabelDominanceType::NONE:
            return std::unique_ptr<DominanceRelation>(
                new DominanceRelationSimple<LabelRelationIdentity>(labels.get()));
        case LabelDominanceType::NOOP:
            return std::unique_ptr<DominanceRelation>(
                new DominanceRelationSimple<LabelRelationNoop>(labels.get()));
        case LabelDominanceType::ALTERNATIVE:
            return std::unique_ptr<DominanceRelation>(
                new DominanceRelationSimple<AlternativeLabelRelation>(labels.get()));
        case LabelDominanceType::NORMAL:
            if (labels->get_size() > switch_off_label_dominance) {
                return std::unique_ptr<DominanceRelation>(
                    new DominanceRelationSimple<LabelRelationNoop>(labels.get()));
            }
            return std::unique_ptr<DominanceRelation>(
                new DominanceRelationSimple<LabelRelation>(labels.get()));
        default:
            std::cerr << "Error: unkown type of simulation relation or label dominance" << std::endl;
            exit(-1);
        }
    default:
        std::cerr << "Error: unkown type of simulation relation or label dominance" << std::endl;
        exit(-1);
    }
}

template<typename T>
void LDSimulation::compute_numeric_dominance_relation(int truncate_value,
                                                      int max_simulation_time,
                                                      int min_simulation_time, int max_total_time,
                                                      int max_lts_size_to_compute_simulation,
                                                      int num_labels_to_use_dominates_in,
                                                      bool dump,
                                                      std::shared_ptr<TauLabelManager<T>> tau_label_mgr,
                                                      std::unique_ptr<NumericDominanceRelation<T>> &result) const {
    result = std::make_unique<NumericDominanceRelation<T>>(labels.get(),
                                                           truncate_value, max_simulation_time,
                                                           min_simulation_time, max_total_time,
                                                           max_lts_size_to_compute_simulation,
                                                           num_labels_to_use_dominates_in,
                                                           tau_label_mgr);

    LabelMap labelMap(labels.get());

    std::vector<Abstraction *> abstractions_not_null;

    std::vector<LabelledTransitionSystem *> ltss_simple;
    // Generate LTSs and initialize simulation relations
    DEBUG_MSG(std::cout << "Building LTSs and Simulation Relations:";
              );
    for (auto a: abstractions) {
        if (!a) {
            continue;
        }
        abstractions_not_null.push_back(a);
        a->compute_distances();
        if (!a->is_solvable()) {
            utils::exit_with(utils::ExitCode::SEARCH_UNSOLVABLE);
        }
        int lts_size, lts_trs;
        ltss_simple.push_back(a->get_lts(labelMap));
        lts_size = ltss_simple.back()->size();
        lts_trs = ltss_simple.back()->num_transitions();
        DEBUG_MSG(std::cout << " " << lts_size << " (" << lts_trs << ")";
                  );
    }
    DEBUG_MSG(std::cout << std::endl;
              );


    result->init(abstractions_not_null);
    result->compute_ld_simulation(ltss_simple, labelMap, dump);
}


template
void LDSimulation::compute_numeric_dominance_relation(int truncate_value,
                                                      int max_simulation_time,
                                                      int min_simulation_time, int max_total_time,
                                                      int max_lts_size_to_compute_simulation,
                                                      int num_labels_to_use_dominates_in,
                                                      bool dump,
                                                      std::shared_ptr<TauLabelManager<int>> tau_label_mgr,
                                                      std::unique_ptr<NumericDominanceRelation<int>> &result) const;


template
void LDSimulation::compute_numeric_dominance_relation(int truncate_value,
                                                      int max_simulation_time,
                                                      int min_simulation_time, int max_total_time,
                                                      int max_lts_size_to_compute_simulation,
                                                      int num_labels_to_use_dominates_in,
                                                      bool dump,
                                                      std::shared_ptr<TauLabelManager<IntEpsilon>> tau_label_mgr,
                                                      std::unique_ptr<NumericDominanceRelation<IntEpsilon>> &result)
const;


void LDSimulation::init_atomic_abstractions() {
    std::cout << "Init atomic abstractions" << std::endl;
    Abstraction::build_atomic_abstractions(abstractions, labels.get());
    if (!useless_vars.empty())
        remove_useless_atomic_abstractions(abstractions);
    for (auto abs : abstractions) {
        // normalize here is necessary, as otherwise compute_distances might remove more transitions than it should (e.g., in nomystery-opt11:p06)
        abs->normalize();
        abs->compute_distances();
    }
    remove_dead_labels(abstractions);
}

void LDSimulation::init_factored_systems(const std::vector<std::vector<int>> &partition_vars) {
    for (const auto &factor: partition_vars) {
        auto *abs_factor = new PDBAbstraction(labels.get(), factor);
        abstractions.push_back(abs_factor);
        abs_factor->normalize();
        abs_factor->compute_distances();
    }
}

void LDSimulation::remove_dead_labels(std::vector<Abstraction *> &_abstractions) {
    std::vector<int> new_dead_labels;
    dead_labels.resize(labels->get_size(), false);

    for (int label_no = 0; label_no < global_simulation_task()->get_num_operators(); label_no++) {
        if (!dead_labels[label_no] && is_dead(label_no)) {
            new_dead_labels.push_back(label_no);
        }
    }

    for (auto abs: _abstractions) {
        if (abs)
            abs->get_dead_labels(dead_labels, new_dead_labels);
    }

    if (!new_dead_labels.empty()) {
        std::set<Abstraction *> recompute_distances;
        std::cout << "Removing dead labels: " << new_dead_labels.size() << std::endl;
        for (auto l: new_dead_labels) {
            for (auto abs: _abstractions) {
                if (abs && abs->prune_transitions_dominated_label_all(l) > 0) {
                    recompute_distances.insert(abs);
                }
            }
        }

        for (auto abs : recompute_distances) {
            abs->compute_distances();
            abs->reset_lts();
        }
    }
}

int LDSimulation::remove_useless_abstractions(std::vector<Abstraction *> &_abstractions) {
    remove_dead_labels(_abstractions);
    if (dominance_relation)
        dominance_relation->remove_useless();
    int removed_abstractions = 0;
    for (int i = 0; i < _abstractions.size(); ++i) {
        if (_abstractions[i] && _abstractions[i]->is_useless()) {
            useless_vars.insert(std::end(useless_vars), std::begin(_abstractions[i]->get_varset()),
                                std::end(_abstractions[i]->get_varset()));
            _abstractions[i]->release_memory();
            //TODO: check if I should do labels->set_irrelevant_for_all_labels(abstractions[i]);
            delete _abstractions[i];
            _abstractions[i] = nullptr;

            removed_abstractions++;
        }
    }
    return removed_abstractions;
}


double LDSimulation::estimated_memory_MB(const std::vector<Abstraction *> &all_abstractions) {
    double total_mem = 0;
    for (auto abs: all_abstractions) {
        if (abs)
            total_mem += abs->memory_estimate() / (1024.0 * 1024.0);
    }
    std::cout << "Total mem: " << total_mem << std::endl;
    return total_mem;
}

// Just main loop copied from merge_and_shrink heuristic
void LDSimulation::complete_heuristic(MergeStrategy *merge_strategy, ShrinkStrategy *shrink_strategy,
                                      bool shrink_after_merge, int limit_seconds, int limit_memory_kb,
                                      bool prune_dead_operators, bool use_expensive_statistics,
                                      std::vector<std::unique_ptr<Abstraction>> &res) const {
    utils::Timer t_mas;
    std::cout << "Complete heuristic Initialized with " << abstractions.size() << " abstractions" << std::endl;
    //Insert atomic abstractions in the first g_variable_domain
    //variables, filling with nullptr. Then composite abstractions
    std::vector<Abstraction *> all_abstractions(global_simulation_task()->get_num_variables(), nullptr);
    int remaining_abstractions = 0;
    for (auto a: abstractions) {
        remaining_abstractions++;
        if (a->get_varset().size() == 1) {
            all_abstractions[*(a->get_varset().begin())] = a->clone();
        } else {
            all_abstractions.push_back(a->clone());
        }
    }
    labels->reset_relevant_for(all_abstractions);

    std::vector<int> used_vars;
    for (int i = 0; i < global_simulation_task()->get_num_variables(); i++) {
        if (!all_abstractions[i])
            used_vars.push_back(i);
    }

    if (merge_strategy) {
        merge_strategy->init(all_abstractions);
        merge_strategy->remove_useless_vars(used_vars);
    }

    if (abstractions.size() > 1) {
        labels->reduce(std::make_pair(0, 1), all_abstractions);
// With the reduction methods we use here, this should just apply label reduction on all abstractions
    }

    while (merge_strategy && !merge_strategy->done() && remaining_abstractions > 1 &&
           t_mas() < limit_seconds && utils::get_peak_memory_in_kb() < limit_memory_kb) {
        std::cout << std::endl << "Remaining: " << remaining_abstractions <<
            " time: " << t_mas() << "/" << limit_seconds << "s" <<
            " memory: " << utils::get_peak_memory_in_kb() << "/" << limit_memory_kb << " KB" << std::endl;

        remaining_abstractions--;
        std::pair<int, int> next_systems = merge_strategy->get_next(all_abstractions);
        int system_one = next_systems.first;
        int system_two = next_systems.second;
        DEBUG_MAS(std::cout << " NEXT SYSTEMS: " << system_one << " " << system_two << std::endl;
                  );
        assert(system_one != system_two);


        Abstraction *abstraction = all_abstractions[system_one];
        assert(abstraction);
        Abstraction *other_abstraction = all_abstractions[system_two];
        assert(other_abstraction);


        if (shrink_strategy && !shrink_after_merge) {
            // Note: we do not reduce labels several times for the same abstraction
            bool reduced_labels = false;
            if (shrink_strategy->reduce_labels_before_shrinking()) {
                labels->reduce(std::make_pair(system_one, system_two), all_abstractions);
                reduced_labels = true;
                abstraction->normalize();
                other_abstraction->normalize();
                abstraction->statistics(use_expensive_statistics);
                other_abstraction->statistics(use_expensive_statistics);
            }

            // distances need to be computed before shrinking
            abstraction->compute_distances();
            other_abstraction->compute_distances();
            if (!abstraction->is_solvable()) {
                utils::exit_with(utils::ExitCode::SEARCH_UNSOLVABLE);
            }
            if (!other_abstraction->is_solvable()) {
                utils::exit_with(utils::ExitCode::SEARCH_UNSOLVABLE);
            }

            shrink_strategy->shrink_before_merge(*abstraction, *other_abstraction);
            // TODO: Make shrink_before_merge return a pair of bools
            //       that tells us whether they have actually changed,
            //       and use that to decide whether to dump statistics?
            // (The old code would print statistics on abstraction iff it was
            // shrunk. This is not so easy any more since this method is not
            // in control, and the shrink strategy doesn't know whether we want
            // expensive statistics. As a temporary aid, we just print the
            // statistics always now, whether or not we shrunk.)
            std::cout << "M1: ";
            abstraction->statistics(use_expensive_statistics);
            std::cout << "M2: ";
            other_abstraction->statistics(use_expensive_statistics);

            if (!reduced_labels) {
                labels->reduce(std::make_pair(system_one, system_two), all_abstractions);
            }
            abstraction->normalize();
            other_abstraction->normalize();

            abstraction->compute_distances();
            other_abstraction->compute_distances();

            DEBUG_MAS(
                if (!reduced_labels) {
                    // only print statistics if we just possibly reduced labels
                    other_abstraction->statistics(use_expensive_statistics);
                    abstraction->statistics(use_expensive_statistics);
                }
                );
        } else {
            abstraction->normalize();
            other_abstraction->normalize();
        }


        Abstraction *new_abstraction = new CompositeAbstraction(labels.get(),
                                                                abstraction,
                                                                other_abstraction);

        abstraction->release_memory();
        other_abstraction->release_memory();

        std::cout << "Merged: ";
        new_abstraction->statistics(use_expensive_statistics);

        all_abstractions[system_one] = nullptr;
        all_abstractions[system_two] = nullptr;
        all_abstractions.push_back(new_abstraction);

        /* this can help pruning unreachable/irrelevant states before starting on label reduction
         * problem before: label reduction ran out of memory if unreachable/irrelevant states not
         * pruned beforehand (at least in some instances)
         * possible downside: Too many transitions here
         */
        new_abstraction->compute_distances();
        if (!new_abstraction->is_solvable()) {
            utils::exit_with(utils::ExitCode::SEARCH_UNSOLVABLE);
        }

        if (shrink_strategy && shrink_after_merge) {
            labels->reduce(std::make_pair(all_abstractions.size() - 1,
                                          all_abstractions.size() - 1), all_abstractions);
            new_abstraction->normalize();
            shrink_strategy->shrink(*new_abstraction, std::numeric_limits<int>::max(), true);
            assert(new_abstraction->is_solvable());
        }
    }

    for (auto &abstraction: all_abstractions) {
        if (abstraction) {
            abstraction->compute_distances();

            std::cout << "Final: ";
            abstraction->statistics(use_expensive_statistics);

            if (!abstraction->is_solvable()) {
                utils::exit_with(utils::ExitCode::SEARCH_UNSOLVABLE);
            }

            res.push_back(std::unique_ptr<Abstraction>(abstraction));
        }
    }

    if (prune_dead_operators)
        prune_dead_ops(all_abstractions);

    for (auto &all_abstraction: all_abstractions) {
        if (all_abstraction) {
            all_abstraction->release_memory();
        }
    }
}


int LDSimulation::remove_useless_atomic_abstractions(std::vector<Abstraction *> &abss) const {
    int total = 0;
    for (auto &abs: abss) {
        if (abs) {
            const std::vector<int> &abs_varset = abs->get_varset();
            if (abs_varset.size() == 1) {
                if (std::find(begin(useless_vars), end(useless_vars), abs_varset[0]) != end(useless_vars)) {
                    total++;
                    abs = nullptr;
                }
            }
        }
    }
    return total;
}

void LDSimulation::build_abstraction(MergeStrategy *merge_strategy, int limit_absstates_merge,
                                     int min_limit_absstates_merge,
                                     int limit_transitions_merge, bool original_merge,
                                     ShrinkStrategy *shrink_strategy, bool forbid_lr,
                                     int limit_seconds, int limit_memory_kb,
                                     bool intermediate_simulations, bool incremental_simulations,
                                     SimulationType simulation_type,
                                     LabelDominanceType label_dominance_type,
                                     int switch_off_label_dominance, bool complex_lts,
                                     bool apply_subsumed_transitions_pruning, bool apply_label_dominance_reduction,
                                     bool apply_simulation_shrinking, bool preserve_all_optimal_plans,
                                     bool use_expensive_statistics) {
    // TODO: We're leaking memory here in various ways. Fix this.
    //       Don't forget that build_atomic_abstractions also
    //       allocates memory.
    utils::Timer t;

    int remaining_abstractions = 0;
    std::vector<Abstraction *> all_abstractions;
    if (abstractions.empty()) {
        // vector of all abstractions. entries with 0 have been merged.
        all_abstractions.reserve(global_simulation_task()->get_num_variables() * 2 - 1);
        Abstraction::build_atomic_abstractions(all_abstractions, labels.get());
        remaining_abstractions = all_abstractions.size();

        if (!useless_vars.empty()) {
            remaining_abstractions -= remove_useless_atomic_abstractions(abstractions);
        }
    } else {
        all_abstractions.resize(global_simulation_task()->get_num_variables(), nullptr);
        for (auto a: abstractions) {
            remaining_abstractions++;
            if (a->get_varset().size() == 1) {
                all_abstractions[*(a->get_varset().begin())] = a->clone();
            } else {
                all_abstractions.push_back(a->clone());
            }
        }
        labels->reset_relevant_for(all_abstractions);
        abstractions.clear();
    }

    std::vector<int> used_vars;
    for (int i = 0; i < global_simulation_task()->get_num_variables(); i++) {
        if (!all_abstractions[i])
            used_vars.push_back(i);
    }

    // TODO (Jan) check putting this into if clause
    if (merge_strategy) {
        merge_strategy->init(all_abstractions);
        merge_strategy->remove_useless_vars(used_vars);
    }


    // compute initial simulations, based on atomic abstractions
    if (intermediate_simulations) {
        if (!forbid_lr) {
            DEBUG_MAS(std::cout << "Reduce labels: " << labels->get_size() << " t: " << t() << std::endl;
                      );
            // With the reduction methods we use here, this should
            // just apply label reduction on all abstractions
            labels->reduce(std::make_pair(0, 1), all_abstractions);
            DEBUG_MAS(std::cout << "Normalize: " << t() << std::endl;
                      );
            for (auto abs: all_abstractions) {
                if (abs) {
                    abs->normalize();
                    DEBUG_MAS(abs->statistics(use_expensive_statistics);
                              );
                }
            }
        }
        for (auto &all_abstraction: all_abstractions) {
            if (all_abstraction) {
                abstractions.push_back(all_abstraction);
            }
        }

        // initialize simulations
        compute_ld_simulation(simulation_type, label_dominance_type, switch_off_label_dominance,
                              complex_lts,
                              apply_subsumed_transitions_pruning,
                              apply_label_dominance_reduction,
                              apply_simulation_shrinking, preserve_all_optimal_plans,
                              /*incremental_step*/ false, /*dump*/ false);
    } else if (shrink_strategy) {
        if (!forbid_lr) {
            DEBUG_MAS(std::cout << "Reduce labels: " << labels->get_size() << " t: " << t() << std::endl;
                      );
            std::cout << "Reduce labels: " << labels->get_size() << " t: " << t() << std::endl;
            labels->reduce(std::make_pair(0, 1), all_abstractions);
            // With the reduction methods we use here,
            //this should just apply label reduction on all abstractions
            DEBUG_MAS(std::cout << "Normalize: " << t() << std::endl;
                      );
            for (auto abs: all_abstractions) {
                if (abs) {
                    abs->normalize();
                    DEBUG_MAS(abs->statistics(use_expensive_statistics);
                              );
                }
            }
        }
        // do not use bisimulation shrinking on atomic abstractions if simulations are used
        DEBUG_MAS(std::cout << "Bisimulation-shrinking atomic abstractions..." << std::endl;
                  );
        for (auto &abstraction: all_abstractions) {
            if (abstraction) {
                abstraction->compute_distances();
                if (!abstraction->is_solvable()) {
                    utils::exit_with(utils::ExitCode::SEARCH_UNSOLVABLE);
                }
                shrink_strategy->shrink_atomic(*abstraction);
            }
        }
    }


    remaining_abstractions -= remove_useless_abstractions(all_abstractions);

    DEBUG_MAS(std::cout << "Merging abstractions..." << std::endl;
              );
    if (merge_strategy) {
        merge_strategy->remove_useless_vars(useless_vars);
    }
    while (merge_strategy && !merge_strategy->done() && t() <= limit_seconds &&
           utils::get_peak_memory_in_kb() < limit_memory_kb &&
           remaining_abstractions > 1) {
        std::cout << std::endl << "Remaining: " << remaining_abstractions <<
            " time: " << t() << "/" << limit_seconds << "s" <<
            " memory: " << utils::get_peak_memory_in_kb() << "/" << limit_memory_kb << " KB" << std::endl;

        std::pair<int, int> next_systems = original_merge ? merge_strategy->get_next(all_abstractions) :
            merge_strategy->get_next(all_abstractions, limit_absstates_merge,
                                     min_limit_absstates_merge,
                                     limit_transitions_merge);
        int system_one = next_systems.first;
        if (system_one == -1) {
            break;         //No pairs to be merged under the limit
        }
        Abstraction *abstraction = all_abstractions[system_one];
        assert(abstraction);
        int system_two = next_systems.second;
        assert(system_one != system_two);
        Abstraction *other_abstraction = all_abstractions[system_two];
        assert(other_abstraction);

        if (original_merge) {
            if ((limit_absstates_merge &&
                 abstraction->size() * other_abstraction->size() > limit_absstates_merge) ||
                (limit_transitions_merge &&
                 abstraction->estimate_transitions(other_abstraction) > limit_transitions_merge
                 && !(min_limit_absstates_merge &&
                      abstraction->size() * other_abstraction->size() <= min_limit_absstates_merge))) {
                break;
            }
        }
        DEBUG_MAS(std::cout << "Merge: " << t() << std::endl;
                  );

        std::cout << "M1: ";
        abstraction->statistics(use_expensive_statistics);
        std::cout << "M2: ";
        other_abstraction->statistics(use_expensive_statistics);

        //TODO: Could be improved by updating simulation when doing incremental simulation
        auto *new_abstraction = new CompositeAbstraction(labels.get(), abstraction, other_abstraction);

        abstraction->release_memory();
        other_abstraction->release_memory();

        remaining_abstractions--;
        std::cout << "Merged: ";
        new_abstraction->statistics(use_expensive_statistics);


        all_abstractions[system_one] = nullptr;
        all_abstractions[system_two] = nullptr;
        all_abstractions.push_back(new_abstraction);

        // Note: we do not reduce labels several times for the same abstraction
        bool reduced_labels = false;
        if (shrink_strategy && shrink_strategy->reduce_labels_before_shrinking()) {
            remove_dead_labels(all_abstractions);
            if (!forbid_lr) {
                DEBUG_MAS(std::cout << "Reduce labels: " << labels->get_size() << " t: " << t() << std::endl;
                          );
                //labels->reduce(make_pair(system_one, system_two), all_abstractions);
                if (remaining_abstractions == 1) {
                    labels->reduce_to_cost();
                } else {
                    // With the reduction methods we use here, this should just apply label reduction on all abstractions
                    labels->reduce(std::make_pair(0, 1), all_abstractions);
                }
                reduced_labels = true;
            }
            DEBUG_MAS(std::cout << "Normalize: " << t() << std::endl;
                      );
            new_abstraction->normalize();
            DEBUG_MAS(new_abstraction->statistics(use_expensive_statistics);
                      );
        }

        DEBUG_MAS(std::cout << "Compute distances: " << t() << std::endl;
                  );
        // distances need to be computed before shrinking
        //abstraction->compute_distances();
        //other_abstraction->compute_distances();
        new_abstraction->compute_distances();
        if (!new_abstraction->is_solvable()) {
            utils::exit_with(utils::ExitCode::SEARCH_UNSOLVABLE);
        }

        if ((shrink_strategy || intermediate_simulations || apply_subsumed_transitions_pruning) &&
            !reduced_labels) {
            remove_dead_labels(all_abstractions);
            if (!forbid_lr) {
                if (remaining_abstractions == 1) {
                    labels->reduce_to_cost();
                } else {
                    labels->reduce(std::make_pair(0, 1),
                                   all_abstractions);         // With the reduction methods we use here, this should just apply label reduction on all abstractions
                }
            }
            for (auto a: all_abstractions)
                if (a)
                    a->normalize();
        } else {
            DEBUG_MAS(std::cout << "Normalize: " << t() << std::endl;
                      );
            //abstraction->normalize();
            //other_abstraction->normalize();
            new_abstraction->normalize();
        }

        if (shrink_strategy) {
            /* PIET-edit: Well, we actually want to have bisim shrinking AFTER merge */
            DEBUG_MAS(std::cout << "Shrink: " << t() << std::endl;
                      );
            shrink_strategy->shrink(*new_abstraction, new_abstraction->size(),
                                    true);         /* force bisimulation shrinking */

            new_abstraction->normalize();
            //abstraction->statistics(use_expensive_statistics);
            //other_abstraction->statistics(use_expensive_statistics);
            DEBUG_MAS(new_abstraction->statistics(use_expensive_statistics);
                      );
        }


        new_abstraction->compute_distances();

        if (!reduced_labels) {
            // only print statistics if we just possibly reduced labels
            //other_abstraction->statistics(use_expensive_statistics);
            //abstraction->statistics(use_expensive_statistics);
            DEBUG_MAS(new_abstraction->statistics(use_expensive_statistics);
                      );
        }

        DEBUG_MAS(std::cout << "Next it: " << t() << std::endl;
                  );
        if (intermediate_simulations) {
            /* PIET-edit: What to do in case of incremental calculations? My guess is: Same as with abstractions.
             * That is, set the simulation relations corresponding to the just merged abstractions to null pointers
             * and in compute_ld_simulation add a single new simulation relation at the end of the vector.
             */
            abstractions.clear();
            for (auto &all_abstraction: all_abstractions) {
                if (all_abstraction) {
                    abstractions.push_back(all_abstraction);
                }
            }

            if (incremental_simulations) {
                dominance_relation->
                init_incremental(new_abstraction,
                                 abstraction->get_simulation_relation(),
                                 other_abstraction->get_simulation_relation());
            }
            compute_ld_simulation(simulation_type, label_dominance_type,
                                  switch_off_label_dominance, complex_lts,
                                  apply_subsumed_transitions_pruning,
                                  apply_label_dominance_reduction,
                                  apply_simulation_shrinking, preserve_all_optimal_plans,
                                  incremental_simulations, /*dump*/ false);
        }

        remaining_abstractions -= remove_useless_abstractions(all_abstractions);
    }

    if (intermediate_simulations) {
        abstractions.clear();
    }
    for (auto &all_abstraction: all_abstractions) {
        if (all_abstraction) {
            all_abstraction->compute_distances();
            DEBUG_MAS(all_abstraction->statistics(use_expensive_statistics);
                      );
            abstractions.push_back(all_abstraction);
        }
    }

    DEBUG_MAS(std::cout << "Partition: " << std::endl;
              for (auto a: abstractions) {
            const auto &varset = a->get_varset();
            int size = 1;
            for (int v: varset) {
                std::cout << " " << v << "("
                          << global_simulation_task()->get_fact_name(FactPair(v, 0)) << ")";
                size *= global_simulation_task()->get_variable_domain_size(v);
            }
            std::cout << " (" << size << ")" << std::endl;
        }
              std::cout << std::endl;
              );
}


void LDSimulation::compute_ld_simulation(SimulationType simulation_type,
                                         LabelDominanceType label_dominance_type,
                                         int switch_off_label_dominance, bool /*complex_lts*/,
                                         bool apply_subsumed_transitions_pruning,
                                         bool apply_label_dominance_reduction,
                                         bool apply_simulation_shrinking,
                                         bool preserve_all_optimal_plans,
                                         bool incremental_step, bool dump) {
    if (!dominance_relation) {
        dominance_relation =
            create_dominance_relation(simulation_type, label_dominance_type, switch_off_label_dominance);
    }

    LabelMap labelMap(labels.get());

    std::vector<LabelledTransitionSystem *> ltss_simple;
    //vector<LTSComplex *> ltss_complex;
    // Generate LTSs and initialize simulation relations
    DEBUG_MSG(std::cout << "Building LTSs and Simulation Relations:";
              );
    for (auto a: abstractions) {
        a->compute_distances();
        if (!a->is_solvable()) {
            utils::exit_with(utils::ExitCode::SEARCH_UNSOLVABLE);
        }
        int lts_size, lts_trs;
        //if (complex_lts) {
        //     ltss_complex.push_back(a->get_lts_complex(labelMap));
        //     lts_size= ltss_complex.back()->size();
        //     lts_trs= ltss_complex.back()->num_transitions();
        //} else {
        ltss_simple.push_back(a->get_lts(labelMap));
        lts_size = ltss_simple.back()->size();
        lts_trs = ltss_simple.back()->num_transitions();
        //}
        DEBUG_MSG(std::cout << " " << lts_size << " (" << lts_trs << ")";
                  );
    }
    DEBUG_MSG(std::cout << std::endl;
              );

    if (!incremental_step) {
        dominance_relation->init(abstractions);
    }

    // Algorithm to compute LD simulation
    //if (complex_lts) {
    // dominance_relation->compute_ld_simulation(ltss_complex, labelMap,
    //                                        incremental_step, dump);
    //} else {
    dominance_relation->compute_ld_simulation(ltss_simple, labelMap, incremental_step, dump);
    //}


    if (apply_subsumed_transitions_pruning) {
        utils::Timer t;
        int lts_id = incremental_step ? dominance_relation->size() - 1 : -1;

        DEBUG_MAS(std::cout << "number of transitions before pruning:" << std::endl;
                  for (auto abs: abstractions) {
                abs->statistics(false);
            }
                  );
        int num_pruned_trs = dominance_relation->prune_subsumed_transitions(abstractions, labelMap, ltss_simple,
                                                                            lts_id /*TODO: Hack lts_complex will not work ever */,
                                                                            preserve_all_optimal_plans);

        remove_dead_labels(abstractions);


        if (num_pruned_trs) {
            std::cout << num_pruned_trs << " transitions pruned from LTS " << lts_id << ". ";
        }

        //_labels->prune_irrelevant_labels();
    }

    if (apply_label_dominance_reduction) {
        std::set<int> dangerous_LTSs;
        //labels->reduce(make_pair(0, 1), abstractions);

        labels->reduce(labelMap, *dominance_relation, dangerous_LTSs);
        DEBUG_MAS(std::cout << "Labels reduced. Dangerous for: " << dangerous_LTSs.size() << std::endl;
                  );
        //for (auto v : dangerous_LTSs) cout << v << " ";
        //cout << endl;

        for (auto abs: abstractions) {
            // normalize here is necessary, as otherwise
            // compute_distances might remove more transitions than it
            // should (e.g., in nomystery-opt11:p06)
            abs->normalize();
        }

        if (apply_simulation_shrinking) {
            if (incremental_step) {
                // Should be enough to just shrink the new abstraction (using the new simulation relation).
                if (!dangerous_LTSs.count(dominance_relation->size() - 1)) {
                    //TODO: HACK HACK we should refractor a bit this.
                    dominance_relation->get_simulations().back()->shrink();
                }
            } else {
                //cout << "Shrink all" << endl;
                for (int i = 0; i < dominance_relation->size(); i++) {
                    if (!dangerous_LTSs.count(i)) {
                        (*dominance_relation)[i].shrink();
                    }
                }
            }
        }
    }

    remove_dead_labels(abstractions);

    for (auto abs: abstractions) {
        // normalize here is necessary, as otherwise compute_distances might remove more transitions than it should (e.g., in nomystery-opt11:p06)
        abs->normalize();
        abs->compute_distances();
    }


    if (!abstractions.empty())
        std::cout << abstractions.back()->get_num_nonreduced_labels() << " / " <<
            abstractions.back()->get_num_labels() << " labels still alive. " << std::endl;
    DEBUG_MAS(std::cout << "Final LTSs: ";
              );
    for (auto abs: abstractions) {
        abs->normalize();
        DEBUG_MAS(std::cout << abs->size() << " (" << abs->total_transitions() << ") ";
                  );
    }
    DEBUG_MAS(std::cout << std::endl << std::endl;
              );
}

void LDSimulation::compute_final_simulation(SimulationType simulation_type,
                                            LabelDominanceType label_dominance_type,
                                            int switch_off_label_dominance,
                                            bool intermediate_simulations, bool complex_lts,
                                            bool apply_subsumed_transitions_pruning,
                                            bool apply_label_dominance_reduction,
                                            bool apply_simulation_shrinking,
                                            bool preserve_all_optimal_plans, bool dump) {
    std::cout << "Computing simulation..." << std::endl;
    if (!dominance_relation) {
        dominance_relation =
            create_dominance_relation(simulation_type, label_dominance_type, switch_off_label_dominance);
    } else if (intermediate_simulations) {
        // remove the intermediately built simulations, and create the final ones from scratch
        dominance_relation->clear();
    }
    compute_ld_simulation(simulation_type, label_dominance_type, switch_off_label_dominance,
                          complex_lts,
                          apply_subsumed_transitions_pruning,
                          apply_label_dominance_reduction,
                          apply_simulation_shrinking, preserve_all_optimal_plans,
                          /*incremental_simulations*/ false, dump);
    std::cout << std::endl;
    std::cout << "Done initializing simulation heuristic [" << utils::g_timer << "]"
              << std::endl;

    std::cout << "Final abstractions: " << abstractions.size() << std::endl;
    for (auto abs: abstractions) {
        abs->normalize();
        const std::vector<int> &varset = abs->get_varset();
        std::cout << "   " << varset.size() << " variables " << abs->size() << " states "
                  << abs->total_transitions()
                  << " transitions " << std::endl;
        DEBUG_MAS(std::cout << "used variables:";
                  for (auto var: varset) {
                std::cout << " " << var;
            }
                  std::cout << std::endl;
                  );
    }


    dominance_relation->dump_statistics(false);
    if (!useless_vars.empty())
        std::cout << "Useless vars: " << useless_vars.size() << std::endl;
}

void LDSimulation::prune_dead_ops(const std::vector<Abstraction *> &all_abstractions) const {
    std::vector<bool> dead_labels_ops(labels->get_size(), false);
    std::vector<bool> dead_operators(global_simulation_task()->get_num_operators(), false);
    for (auto abs: all_abstractions)
        if (abs)
            abs->check_dead_operators(dead_labels_ops, dead_operators);
    int num_dead = 0;
    int were_dead = 0;
    for (int i = 0; i < dead_operators.size(); i++) {
        if (!is_dead(i)) {
            if (dead_operators[i])
                num_dead++;
        } else {
            were_dead++;
        }
    }

    std::cout << "Dead operators due to dead labels: " << (were_dead + num_dead) <<
        "(new " << num_dead << ") " << global_simulation_task()->get_num_operators() << " / " <<
        (((double)(num_dead + were_dead) / global_simulation_task()->get_num_operators()) * 100);

    if (!Abstraction::store_original_operators) {
        for (int i = 0; i < global_simulation_task()->get_num_operators(); i++) {
            if (dead_operators[i]) {
                set_dead(i);
            }
        }
    } else {
        boost::dynamic_bitset<> required_operators(global_simulation_task()->get_num_operators());
        for (int i = 0; i < labels->get_size(); i++) {
            if (dead_labels[i] || labels->is_label_reduced(i) ||
                (i < global_simulation_task()->get_num_operators() && is_dead(i))) {
                continue;
            }

            boost::dynamic_bitset<> required_operators_for_label;
            bool irrelevant_for_all_abstractions = true;
            for (auto abs: all_abstractions) {
                if (!abs || !abs->get_relevant_labels()[i])
                    continue;
                irrelevant_for_all_abstractions = false;
                const std::vector<AbstractTransition> &transitions = abs->get_transitions_for_label(i);
                const auto &t_ops = abs->get_transition_ops_for_label(i);

                boost::dynamic_bitset<> required_operators_for_abstraction(
                    global_simulation_task()->get_num_operators());
                for (int j = 0; j < transitions.size(); j++) {
                    required_operators_for_abstraction |= t_ops[j];
                }
                if (required_operators_for_label.empty()) {
                    required_operators_for_label = required_operators_for_abstraction;
                } else {
                    required_operators_for_label &= required_operators_for_abstraction;
                }
            }
            if (!irrelevant_for_all_abstractions)
                required_operators |= required_operators_for_label;
        }

        std::cout << "Dead operators detected by storing original operators: "
                  << (global_simulation_task()->get_num_operators() - required_operators.count()) << " / " <<
            global_simulation_task()->get_num_operators() << "("
                  << (((double)global_simulation_task()->get_num_operators() - required_operators.count()) /
            global_simulation_task()->get_num_operators() * 100) << ")\n";

        for (int i = 0; i < global_simulation_task()->get_num_operators(); i++) {
            if (!required_operators[i]) {
                set_dead(i);
            }
        }
    }
}


int LDSimulation::get_cost(const State &state) const {
    return dominance_relation->get_cost(state);
}


//Returns a optimized variable ordering that reorders the variables
//according to the standard causal graph criterion
void LDSimulation::getVariableOrdering(std::vector<int> &var_order) const {
    if (abstractions.empty())
        return;
    std::vector<std::vector<int>> partitions;
    std::vector<int> partition_var(global_simulation_task()->get_num_variables(), 0);
    std::cout << "Init partitions" << std::endl;
    std::vector<int> partition_order;

    for (auto a: abstractions) {
        const std::vector<int> &vs = a->get_varset();
        for (int v: vs) {
            partition_var[v] = partitions.size();
        }

        partition_order.push_back(partitions.size());
        partitions.push_back(vs);
    }

    std::cout << "Create IG partitions" << std::endl;

    //First optimize the ordering of the abstractions
    InfluenceGraph ig_partitions(partitions.size());
    const auto &causal_graph = global_simulation_task_proxy()->get_causal_graph();

    for (int v = 0; v < global_simulation_task()->get_num_variables(); v++) {
        for (int v2: causal_graph.get_successors(v)) {
            if (partition_var[v] != partition_var[v2]) {
                ig_partitions.set_influence(partition_var[v], partition_var[v2]);
            }
        }
    }
    std::cout << "Optimize partitions ordering " << std::endl;
    ig_partitions.get_ordering(partition_order);

    std::cout << "Partition ordering: ";
    for (int v: partition_order)
        std::cout << v << " ";
    std::cout << std::endl;

    std::vector<int> partition_begin;
    std::vector<int> partition_size;

    for (int i: partition_order) {
        partition_begin.push_back(var_order.size());
        partition_size.push_back(partitions[i].size());
        for (int v: partitions[i])
            var_order.push_back(v);
    }

    InfluenceGraph ig_vars(global_simulation_task()->get_num_variables());
    for (int v = 0; v < global_simulation_task()->get_num_variables(); v++) {
        for (int v2: causal_graph.get_successors(v)) {
            ig_vars.set_influence(v, v2);
        }
    }

    ig_vars.optimize_variable_ordering_gamer(var_order, partition_begin, partition_size);

    var_order.insert(std::end(var_order), std::begin(useless_vars), std::end(useless_vars));
}

std::vector<int> LDSimulation::getVariableOrdering() const {
    std::vector<int> result;
    getVariableOrdering(result);
    return result;
}


void LDSimulation::release_memory() {
    for (auto &abstraction: abstractions) {
        if (abstraction) {
            abstraction->release_memory();
        }
    }
}

static plugins::TypedEnumPlugin<LabelDominanceType> _enum_plugin1({
        {"NONE", ""},
        {"NOOP", ""},
        {"NORMAL", ""},
        {"ALTERNATIVE", ""}
    });

static plugins::TypedEnumPlugin<SimulationType> _enum_plugin2({
        {"NONE", ""},
        {"SIMPLE", ""}
    });
}
