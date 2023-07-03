#include "numeric_dominance_pruning.h"

#include "../merge_and_shrink/abstraction_builder.h"

#include "numeric_simulation_relation.h"
#include "numeric_dominance_relation.h"

#ifdef INCLUDE_SYM
#include "../sym/sym_transition.h"
#include "../sym/sym_manager.h"
#endif

#include "../../../heuristic.h"

#include <cassert>
#include <vector>

namespace simulations {
#ifdef INCLUDE_SYM
template<typename T>
NumericDominancePruning<T>::NumericDominancePruning(const options::Options &opts)
    :                 // PruneHeuristic(opts),
                      cost_type(opts.get<OperatorCost>("cost_type")),
                      mgrParams(opts),
                      initialized(false),
                      tau_labels(std::make_shared<TauLabelManager<T>>(opts, false)),
                      remove_spurious_dominated_states(opts.get<bool>("remove_spurious")),
                      insert_dominated(opts.get<bool>("insert_dominated")),
                      use_quantified_dominance(opts.get<bool>("use_quantified_dominance")),
                      trade_off_dominance(opts.get<bool>("trade_off_dominance")),
                      only_positive_dominance(opts.get<bool>("only_positive_dominance")),
                      use_ADDs(opts.get<bool>("use_adds")),
                      prune_dominated_by_parent(opts.get<bool>("prune_dominated_by_parent")),
                      prune_dominated_by_initial_state(opts.get<bool>("prune_dominated_by_initial_state")),
                      prune_successors(opts.get<bool>("prune_successors")),
                      prune_dominated_by_closed(opts.get<bool>("prune_dominated_by_closed")),
                      prune_dominated_by_open(opts.get<bool>("prune_dominated_by_open")),
                      truncate_value(opts.get<int>("truncate_value")),
                      max_simulation_time(opts.get<int>("max_simulation_time")),
                      min_simulation_time(opts.get<int>("min_simulation_time")),
                      max_total_time(opts.get<int>("max_total_time")),
                      max_lts_size_to_compute_simulation(opts.get<int>("max_lts_size_to_compute_simulation")),
                      num_labels_to_use_dominates_in(opts.get<int>("num_labels_to_use_dominates_in")),
                      min_insertions_desactivation(opts.get<int>("min_insertions")),
                      min_desactivation_ratio(opts.get<double>("min_desactivation_ratio")),
                      dump(opts.get<bool>("dump")),
                      exit_after_preprocessing(opts.get<bool>("exit_after_preprocessing")),
                      vars(new SymVariables()), abstractionBuilder(opts.get<AbstractionBuilder *>("abs")),
                      all_desactivated(false), activation_checked(false),
                      states_inserted(0), states_checked(0), states_pruned(0), deadends_pruned(0) {
}
#endif

template<typename T>
void NumericDominancePruning<T>::dump_options() const {
    std::cout << "Type pruning: ";
    if (prune_dominated_by_parent) {
        std::cout << " dominated_by_parent";
    }

    if (prune_dominated_by_parent) {
        std::cout << " dominated_by_initial_state";
    }

    if (prune_successors) {
        std::cout << " successors";
    }

    if (prune_dominated_by_closed) {
        std::cout << " dominated_by_closed";
    }

    if (prune_dominated_by_open) {
        std::cout << " dominated_by_open";
    }

    tau_labels->print_config();

    std::cout << "truncate_value: " << truncate_value << std::endl <<
        "num_labels_to_use_dominates_in: " << num_labels_to_use_dominates_in << std::endl <<
        "max_lts_size_to_compute_simulation: " << max_lts_size_to_compute_simulation << std::endl <<
        "max_simulation_time: " << max_simulation_time << std::endl <<
        "min_simulation_time: " << min_simulation_time << std::endl <<
        "max_total_time: " << max_total_time << std::endl;
}


template<typename T>
bool NumericDominancePruning<T>::apply_pruning() const {
    return prune_dominated_by_parent || prune_dominated_by_initial_state || prune_successors ||
           prune_dominated_by_closed || prune_dominated_by_open;
}

template<typename T>
void NumericDominancePruning<T>::initialize(bool force_initialization) {
    if (!initialized) {
        dump_options();
        initialized = true;
        // TODO support cost_type == OperatorCost::ZERO
        abstractionBuilder->build_abstraction(is_unit_cost_task(cost_type),
                                              /* || cost_type == OperatorCost::ZERO, */
                                              cost_type, ldSimulation, abstractions);
        std::cout << "LDSimulation finished" << std::endl;

        if (force_initialization || apply_pruning()) {
            ldSimulation->
            compute_numeric_dominance_relation<T>(truncate_value,
                                                  max_simulation_time,
                                                  min_simulation_time,
                                                  max_total_time,
                                                  max_lts_size_to_compute_simulation,
                                                  num_labels_to_use_dominates_in,
                                                  dump,
                                                  tau_labels,
                                                  numeric_dominance_relation);
        }

        ldSimulation->release_memory();

#ifdef INCLUDE_SYM
        if (prune_dominated_by_closed || prune_dominated_by_open) {
            std::vector<int> var_order = ldSimulation->getVariableOrdering();
            vars->init(var_order, mgrParams);
            if (remove_spurious_dominated_states) {
                mgr = std::unique_ptr<SymManager>(new SymManager(vars.get(), nullptr, mgrParams, cost_type));
                mgr->init();
            }
            mgrParams.print_options();

            numeric_dominance_relation->precompute_bdds(vars.get(), !insert_dominated,
                                                        use_quantified_dominance || trade_off_dominance, use_ADDs);
        }
#endif
        std::cout << "Completed preprocessing: " << utils::g_timer() << std::endl;

        if (exit_after_preprocessing) {
            std::cout << "Exit after preprocessing." << std::endl;
            exit_with(EXIT_UNSOLVED_INCOMPLETE);
        }
    }
}

template<typename T>
bool NumericDominancePruning<T>::is_dead_end(const State &state) {
    if (!prune_dominated_by_parent && !prune_dominated_by_initial_state) {
        for (auto &abs: abstractions) {
            if (abs->is_dead_end(state)) {
                return true;
            }
        }
    }
    return false;
}

template<typename T>
int NumericDominancePruning<T>::compute_heuristic(const State &state) {
    int cost = (ldSimulation && ldSimulation->has_dominance_relation()) ?
        ldSimulation->get_dominance_relation().get_cost(state) : 0;
    if (cost == -1)
        return Heuristic::DEAD_END;

    for (auto &abs: abstractions) {
        if (abs->is_dead_end(state)) {
            return Heuristic::DEAD_END;
        }
        cost = std::max(cost, abs->get_cost(state));
    }

    return cost;
}

//template <typename T>
//void NumericDominancePruning<T>::prune_applicable_operators(const State & state, int /*g*/,
//							 std::vector<const Operator *> & applicable_operators, SearchProgress & search_progress) {
/*    bool applied_action_selection_pruning = false;
    if(prune_successors && applicable_operators.size() > 1) {
	applied_action_selection_pruning = true;
	if(numeric_dominance_relation->action_selection_pruning(state, applicable_operators, search_progress, cost_type)) {
	    return;
	}
    }

    if (prune_dominated_by_parent || prune_dominated_by_initial_state) {
	numeric_dominance_relation->prune_dominated_by_parent_or_initial_state(state, applicable_operators, search_progress, applied_action_selection_pruning, prune_dominated_by_parent, prune_dominated_by_initial_state, cost_type);
    }
}
 */


// template <typename T>
// bool NumericDominancePruning<T>::prune_generation(const State &state, int g,
//						  const State &/*parent*/, int /*action_cost*/ ) {
/*    if(!(prune_dominated_by_open || prune_dominated_by_closed) || !is_activated()) {
	return false;
    }

    // if(states_inserted%1000 == 0){
    //  cout << "Deadend is still activated. "
    //       << "  States inserted: " << states_inserted
    //       << "  Min insertions: " << min_insertions << " " << min_deadends
    //       << "  States pruned: " << states_pruned
    //       << "  Limit pruned: " << states_inserted*min_pruning_ratio
    //       << "  Deadends pruned: " << deadends_pruned
    //       << "  Limit deadends: " << states_inserted*min_deadend_ratio
    //       << "  Prune activated: " << prune_is_activated()
    //       << "  Deadend activated: " << deadend_is_activated()
    //       <<  endl;
    // }

    if(!prune_dominated_by_parent && !prune_dominated_by_initial_state && numeric_dominance_relation->pruned_state(state)){
        return true;
    }

    // if(prune_dominated_by_parent) {
    //  return numeric_dominance_relation->dominates(parent, state, action_cost);
    // }

    // a) Check if state is in a BDD with g.closed <= g
    states_checked ++;
    if (check(state, g)){
        states_pruned ++;
        return true;
    }

    //b) Insert state and other states dominated by it
    if(prune_dominated_by_open){
        insert(state, g);
        states_inserted ++;
    }
    return false;
}
*/

/*
template <typename T>
bool NumericDominancePruning<T>::prune_expansion (const State & state, int g){
    if(!(prune_dominated_by_open || prune_dominated_by_closed) || !is_activated()) {
	return false;
    }

    //a) Check if state is in a BDD with g.closed <= g
    states_checked++;
    if(check(state, g)){
	states_pruned ++;
	return true;
    }
    //b) Insert state and other states dominated by it
    if(prune_dominated_by_closed){
	insert(state, g);
	states_inserted ++;
    }

    return false;
}
 */

#ifdef INCLUDE_SYM
template<typename T>
BDD NumericDominancePruning<T>::getBDDToInsert(const State &state) {
    if (insert_dominated) {
        BDD res = numeric_dominance_relation->getDominatedBDD(vars.get(), state,
                                                              trade_off_dominance);

        if (remove_spurious_dominated_states) {
            // TODO support mutexes
            // res = mgr->filter_mutex(res, true, 1000000, true);
            // res = mgr->filter_mutex(res, false, 1000000, true);
        }

        if (prune_dominated_by_open) {
            res -= vars->getStateBDD(state);         //Remove the state
        } else if (vars->numStates(res) == 1) {
            //Small optimization: If we have a single state, not include it
            return vars->zeroBDD();
        }
        return res;
    } else {
        return vars->getStateBDD(state);
    }
}
#endif

#ifdef INCLUDE_SYM
template<typename T>
static std::shared_ptr<NumericDominancePruning<T>> _parse(options::OptionParser &parser) {
    parser.document_synopsis("Simulation heuristic", "");
    parser.document_language_support("action costs", "supported");
    parser.document_language_support("conditional_effects", "supported (but see note)");
    parser.document_language_support("axioms", "not supported");
    parser.document_property("admissible", "yes");
    parser.document_property("consistent", "yes");
    parser.document_property("safe", "yes");
    parser.document_property("preferred operators", "no");
    parser.document_note(
        "Note",
        "Conditional effects are supported directly. Note, however, that "
        "for tasks that are not factored (in the sense of the JACM 2014 "
        "merge-and-shrink paper), the atomic abstractions on which "
        "merge-and-shrink heuristics are based are nondeterministic, "
        "which can lead to poor heuristics even when no shrinking is "
        "performed.");


    parser.add_option<bool>("dump",
                            "Dumps the relation that has been found",
                            "false");

    parser.add_option<bool>("exit_after_preprocessing",
                            "Exit after preprocessing",
                            "false");

    parser.add_option<bool>("remove_spurious",
                            "If activated, remove spurious states from the sets of simulated/simulating states",
                            "true");

    Heuristic::add_options_to_parser(parser);
    SymParamsMgr::add_options_to_parser_simulation(parser);

    parser.add_option<std::shared_ptr<AbstractionBuilder>>("abs", "abstraction builder", "");
    //LDSimulation::add_options_to_parser(parser);

    parser.add_option<bool>("prune_dominated_by_parent",
                            "Prunes a state if it is dominated by its parent",
                            "false");

    parser.add_option<bool>("prune_dominated_by_initial_state",
                            "Prunes a state if it is dominated by the initial state",
                            "false");

    parser.add_option<bool>("prune_dominated_by_open",
                            "Prunes a state if it is dominated by any node in the closed or open list",
                            "false");

    parser.add_option<bool>("prune_dominated_by_closed",
                            "Prunes a state if it is dominated by any node in the closed list",
                            "false");

    parser.add_option<int>("truncate_value",
                           "Assume -infinity if below minus this value",
                           "1000");

    parser.add_option<int>("max_simulation_time",
                           "Maximum number of seconds spent in computing a single update of a simulation", "1800000");

    parser.add_option<int>("min_simulation_time",
                           "Minimum number of seconds spent in computing a single update of a simulation", "100000");
    // By default we do not have any limit


    parser.add_option<int>("max_total_time",
                           "Maximum number of seconds spent in computing all updates of a simulation", "1800000");

    parser.add_option<int>("num_labels_to_use_dominates_in",
                           "Use dominates_in for instances that have less than this amount of labels",
                           "0");

    parser.add_option<int>("max_lts_size_to_compute_simulation",
                           "Avoid computing simulation on ltss that have more states than this number",
                           "1000000");

    parser.add_option<bool>("prune_successors",
                            "Prunes all siblings if any successor dominates the parent by enough margin",
                            "false");

    parser.add_option<bool>("insert_dominated",
                            "Whether we store the set of dominated states (default) or just the set of closed.",
                            "true");

    parser.add_option<bool>("use_quantified_dominance",
                            "Prune with respect to the quantified or the qualitative dominance",
                            "false");

    parser.add_option<bool>("trade_off_dominance",
                            "Compute dominatedBDD trading off positive and negative values",
                            "false");

    parser.add_option<bool>("only_positive_dominance",
                            "Compute dominatedBDDMaps only for positive values",
                            "false");

    parser.add_option<bool>("use_adds",
                            "Use ADDs (or BDD maps) to represent quantified dominance",
                            "false");


    parser.add_option<bool>("use_single_bdd",
                            "Use a single BDD to represent all dominated states",
                            "false");


    parser.add_option<double>("min_desactivation_ratio",
                              "Ratio of pruned/checked needed to continue pruning the search.",
                              "0.0");

    parser.add_option<int>("min_insertions",
                           "States are inserted and pruning until this limit. Afterwards, depends on the ratios",
                           "1000");

    TauLabelManager<int>::add_options_to_parser(parser);

    options::Options opts = parser.parse();

    auto cost_type = opts.get<OperatorCost>("cost_type");

    // TODO support OperatorCost::ZERO
    bool task_has_zero_cost = cost_type == // OperatorCost::ZERO ||
        (cost_type == OperatorCost::NORMAL &&
         task_properties::get_min_operator_cost(*global_simulation_task_proxy()) == 0);

    if (parser.dry_run()) {
        return 0;
    } else {
        if (opts.get<bool> ("use_adds")) {
            return nullptr; //new NumericDominancePruningADD (opts);
        } else if (opts.get<bool> ("use_single_bdd")) {
            if (opts.get<bool> ("use_quantified_dominance") ||
                opts.get<bool> ("prune_dominated_by_open")) {
                std::cerr <<
                    "Error: you cannot use use_single_bdd and use_quantified_dominance or prune_dominated_by_open" <<
                    std::endl;
                exit_with(EXIT_INPUT_ERROR);
            }

            if (task_has_zero_cost) {
                return std::make_shared<NumericDominancePruningBDD<IntEpsilon>>(opts);
            } else {
                return std::make_shared<NumericDominancePruningBDD<int>> (opts);
            }
        } else {
            if (task_has_zero_cost) {
                return std::make_shared<NumericDominancePruningBDDMap<IntEpsilon>> (opts);
            } else {
                return std::make_shared<NumericDominancePruningBDDMap<int>> (opts);
            }
        }
    }
}
#endif

/*
static Plugin<NumericDominancePruning<int>> _plugin("num_simulation", _parse<int>);
    static Plugin<NumericDominancePruning<IntEpsilon>> _plugin("num_simulation", _parse);
// static Plugin<Heuristic> _plugin_h("num_simulation", _parse_h);
*/

#ifdef INCLUDE_SYM

template<>
std::map<int, BDD> NumericDominancePruning<int>::getBDDMapToInsert(const State &state) {
    assert(insert_dominated);
    std::map<int, BDD> res = numeric_dominance_relation->getDominatedBDDMap(vars.get(), state,
                                                                            only_positive_dominance);

    for (auto it = res.begin(); it != res.end();) {
        BDD &bdd = it->second;
        if (remove_spurious_dominated_states) {
            // TODO support mutexes
            // bdd = mgr->filter_mutex(bdd, true, 1000000, true);
            // bdd = mgr->filter_mutex(bdd, false, 1000000, true);
        }
        if (prune_dominated_by_open) {
            bdd -= vars->getStateBDD(state);         //Remove the state
        }
        if (bdd.IsZero()) {
            res.erase(it++);
        } else {
            ++it;
        }
    }

    if (res.begin()->first < 0) {
        std::map<int, BDD> new_res;
        for (const auto &entry: res) {
            int value = entry.first;
            if (value < 0) {
                value--;
            }
            new_res[value] = entry.second;
        }
        return new_res;
    }
    return res;
}


template<>
std::map<int, BDD> NumericDominancePruning<IntEpsilon>::getBDDMapToInsert(const State &state) {
    assert(insert_dominated);
    std::map<int, BDD> res;
    std::map<IntEpsilon, BDD> dom = numeric_dominance_relation->getDominatedBDDMap(vars.get(), state,
                                                                                   only_positive_dominance);
    for (const auto &it: dom) {
        BDD bdd = it.second;
        if (remove_spurious_dominated_states) {
            // TODO support mutexes
            // bdd = mgr->filter_mutex(bdd, true, 1000000, true);
            // bdd = mgr->filter_mutex(bdd, false, 1000000, true);
        }
        if (prune_dominated_by_open) {
            bdd -= vars->getStateBDD(state);         //Remove the state
        }

        if (bdd.IsZero()) {
            continue;
        }

        int value = it.first.get_value();
        if (value < 0 || it.first.get_epsilon() < 0) {
            value--;
        }
        if (!res.count(value)) {
            res[value] = bdd;
        } else {
            res[value] += bdd;
        }
    }

    return res;
}


template<typename T>
std::map<int, BDD> NumericDominancePruning<T>::getBDDMapToInsert(const State &state) {
    assert(insert_dominated);
    std::map<T, BDD> res = numeric_dominance_relation->getDominatedBDDMap(vars.get(), state);
    for (auto &it: res) {
        BDD &bdd = it.second;
        if (remove_spurious_dominated_states) {
            // TODO support mutexes
            // bdd = mgr->filter_mutex(bdd, true, 1000000, true);
            // bdd = mgr->filter_mutex(bdd, false, 1000000, true);
        }
        if (prune_dominated_by_open) {
            bdd -= vars->getStateBDD(state);         //Remove the state
        }
    }
    return res;
}


template<typename T>
void NumericDominancePruningBDDMap<T>::insert(const State &state, int g) {
    //Timer t;
    if (NumericDominancePruning<T>::use_quantified_dominance) {
        std::map<int, BDD> res = NumericDominancePruning<T>::getBDDMapToInsert(state);

        for (const auto &i: res) {
            int dom_value = i.first;
            int cost = std::max(0, g - dom_value);
            BDD bdd = i.second;
            assert(!bdd.IsZero());
            if (!closed.count(cost)) {
                closed[cost] = bdd;
            } else {
                closed[cost] += bdd;
            }
        }
    } else {
        BDD res = NumericDominancePruning<T>::getBDDToInsert(state);
        if (!closed.count(g)) {
            closed[g] = res;
        } else {
            closed[g] += res;
        }
    }
}


template<typename T>
bool NumericDominancePruningBDDMap<T>::check(const State &state, int g) {
    //Timer t;
    // //CODE TO TEST Simulation TR
    // BDD dominatedBDD = vars->oneBDD();
    // for (auto it = simulations.rbegin(); it != simulations.rend(); it++){
    //   dominatedBDD *= (*it)->getSimulatedBDD(state);
    // }

    // BDD sBDD = vars->getStateBDD(state);
    // BDD trBDD = tr->image(sBDD);
    // if (trBDD != dominatedBDD) {
    //   cerr << "ERROR: the TR does not work " << endl;
    //   exit(0);
    // }

    // //END CODE TO TEST

    if (NumericDominancePruning<T>::insert_dominated) {
        auto sb = NumericDominancePruning<T>::vars->getBinaryDescription(state);
        for (auto &entry: closed) {
            if (entry.first > g)
                break;
            if (!(entry.second.Eval(sb).IsZero())) {
                return true;
            }
        }
    } else {
        BDD simulatingBDD = NumericDominancePruning<T>::numeric_dominance_relation
            ->getDominatingBDD(NumericDominancePruning<T>::vars.get(), state);
        for (auto &entry: closed) {
            if (entry.first > g)
                break;
            if (!((entry.second * simulatingBDD).IsZero())) {
                return true;
            }
        }
    }

    // time_check += t();
    return false;
}

template<typename T>
void NumericDominancePruningBDD<T>::insert(const State &state, int) {
    assert(!NumericDominancePruning<T>::use_quantified_dominance);

    if (!initialized) {
        closed = NumericDominancePruning<T>::vars->zeroBDD();
        closed_inserted = NumericDominancePruning<T>::vars->zeroBDD();
        initialized = true;
    }

    closed += NumericDominancePruning<T>::getBDDToInsert(state);
}

template<typename T>
bool NumericDominancePruningBDD<T>::check(const State &state, int) {
    if (!initialized) {
        closed = NumericDominancePruning<T>::vars->zeroBDD();
        closed_inserted = NumericDominancePruning<T>::vars->zeroBDD();
        initialized = true;
    }

    if (NumericDominancePruning<T>::insert_dominated) {
        auto sb = NumericDominancePruning<T>::vars->getBinaryDescription(state);
        return !(closed.Eval(sb).IsZero());
    } else {
        BDD simulatingBDD = NumericDominancePruning<T>::ldSimulation->get_dominance_relation().getSimulatingBDD(
            NumericDominancePruning<T>::vars.get(), state);
        return !((closed * simulatingBDD).IsZero());
    }
}

template<typename T>
void NumericDominancePruning<T>::print_statistics() {
    if (mgr) {
        std::cout << "Dominance BDD nodes: " << mgr->totalNodes() << std::endl;
        std::cout << "Dominance BDD memory: " << mgr->totalMemory() << std::endl;
    }
}
#endif


template
class NumericDominancePruning<int>;

template
class NumericDominancePruning<IntEpsilon>;

#ifdef INCLUDE_SYM
template
class NumericDominancePruningBDDMap<int>;

template
class NumericDominancePruningBDDMap<IntEpsilon>;

template
class NumericDominancePruningBDD<int>;

template
class NumericDominancePruningBDD<IntEpsilon>;
#endif
}
