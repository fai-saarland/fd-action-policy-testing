#include "satisficing_dominance_pruning.h"

#include "../merge_and_shrink/abstraction.h"
#include "../merge_and_shrink/labels.h"
#include "../merge_and_shrink/labelled_transition_system.h"
#include "../merge_and_shrink/abstraction_builder.h"

#include "numeric_simulation_relation.h"
#include "satisficing_dominance_relation.h"

#include "../../../option_parser.h"
#include "../../../plugin.h"
#include "../../../heuristic.h"

#include "../sym/sym_transition.h"
#include "../sym/sym_manager.h"

#include "../../../utils/timer.h"

#include <cassert>
#include <vector>
#include <limits>

namespace simulations {
SatisficingDominancePruning::SatisficingDominancePruning(const options::Options &opts) :
    // PruneHeuristic(opts),
    cost_type(opts.get<OperatorCost>("cost_type")),
    initialized(false), tau_label_mgr(std::make_shared<TauLabelManager<int>>(opts, true)),
    use_quantified_dominance(opts.get<bool>("use_quantified_dominance")),
    trade_off_dominance(opts.get<bool>("trade_off_dominance")),
    only_positive_dominance(opts.get<bool>("only_positive_dominance")),
    prune_dominated_by_parent(opts.get<bool>("prune_dominated_by_parent")),
    prune_dominated_by_initial_state(opts.get<bool>("prune_dominated_by_initial_state")),
    prune_successors(opts.get<bool>("prune_successors")),
    truncate_value(opts.get<int>("truncate_value")),
    max_simulation_time(opts.get<int>("max_simulation_time")),
    min_simulation_time(opts.get<int>("min_simulation_time")),
    max_total_time(opts.get<int>("max_total_time")),
    max_lts_size_to_compute_simulation(opts.get<int>("max_lts_size_to_compute_simulation")),
    num_labels_to_use_dominates_in(opts.get<int>("num_labels_to_use_dominates_in")),
    dump(opts.get<bool>("dump")),
    exit_after_preprocessing(opts.get<bool>("exit_after_preprocessing")),
    abstractionBuilder(opts.get<AbstractionBuilder *>("abs")) {
}

void SatisficingDominancePruning::dump_options() const {
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

    std::cout << std::endl
              << "truncate_value: " << truncate_value << std::endl <<
        "num_labels_to_use_dominates_in: " << num_labels_to_use_dominates_in << std::endl <<
        "max_lts_size_to_compute_simulation: " << max_lts_size_to_compute_simulation << std::endl <<
        "max_simulation_time: " << max_simulation_time << std::endl <<
        "min_simulation_time: " << min_simulation_time << std::endl <<
        "max_total_time: " << max_total_time << std::endl;

    tau_label_mgr->print_config();
}



bool SatisficingDominancePruning::apply_pruning() const {
    return prune_dominated_by_parent || prune_dominated_by_initial_state || prune_successors;
}


void SatisficingDominancePruning::initialize(bool force_initialization) {
    if (!initialized) {
        dump_options();
        initialized = true;
        // TODO support cost_type == OperatorCost::ZERO
        abstractionBuilder->build_abstraction(is_unit_cost_task(cost_type)
                                              /*|| cost_type == OperatorCost::ZERO */,
                                              cost_type, ldSimulation, abstractions);
        std::cout << "LDSimulation finished" << std::endl;

        if (force_initialization || apply_pruning()) {
            ldSimulation->
            compute_satisficing_dominance_relation(truncate_value,
                                                   max_simulation_time,
                                                   min_simulation_time, max_total_time,
                                                   max_lts_size_to_compute_simulation,
                                                   num_labels_to_use_dominates_in,
                                                   dump, tau_label_mgr,
                                                   numeric_dominance_relation);
        }

        ldSimulation->release_memory();

        std::cout << "Completed preprocessing: " << utils::g_timer() << std::endl;

        if (exit_after_preprocessing) {
            std::cout << "Exit after preprocessing." << std::endl;
            exit_with(EXIT_UNSOLVED_INCOMPLETE);
        }
    }
}

/*
bool SatisficingDominancePruning::is_dead_end(const State &state) {
    if (!prune_dominated_by_parent && !prune_dominated_by_initial_state) {
        for (auto &abs: abstractions) {
            if (abs->is_dead_end(state)) {
                return true;
            }
        }
    }
    return false;
}
 */

/*
bool SatisficingDominancePruning::strictly_dominates_initial_state(const State &new_state) const {
    return numeric_dominance_relation->strictly_dominates_initial_state(new_state);
}
 */

/*
    int SatisficingDominancePruning::compute_heuristic(const State &state) {
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
    */


// void SatisficingDominancePruning::prune_applicable_operators(const State &state, int /*g*/,
//                                                             std::vector<const Operator *> &applicable_operators,
//                                                             SearchProgress &search_progress) {
/*    bool applied_action_selection_pruning = false;
    if (prune_successors && applicable_operators.size() > 1) {
        applied_action_selection_pruning = true;
        if (numeric_dominance_relation->action_selection_pruning(state, applicable_operators, search_progress,
                                                                 cost_type)) {
            return;
        }
    }

    if (prune_dominated_by_parent || prune_dominated_by_initial_state) {
        numeric_dominance_relation->prune_dominated_by_parent_or_initial_state(state, applicable_operators,
                                                                               search_progress,
                                                                               applied_action_selection_pruning,
                                                                               prune_dominated_by_parent,
                                                                               prune_dominated_by_initial_state,
                                                                               cost_type);
    }
}
 */


// bool SatisficingDominancePruning::strictly_dominates (const State & dominating_state,
//                                                    const State & dominated_state) const {
//     return numeric_dominance_relation->strictly_dominates(dominating_state, dominated_state);
//   }


/*
void SatisficingDominancePruning::restart_search(const State &new_initial_state) {
    numeric_dominance_relation->set_initial_state(new_initial_state);
}
 */


static std::shared_ptr<SatisficingDominancePruning> _parse(options::OptionParser &parser) {
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

    add_cost_type_option_to_parser(parser);

    parser.add_option<bool>("dump",
                            "Dumps the relation that has been found",
                            "false");

    parser.add_option<bool>("exit_after_preprocessing",
                            "Exit after preprocessing",
                            "false");


    TauLabelManager<int>::add_options_to_parser(parser);

    Heuristic::add_options_to_parser(parser);

    parser.add_option<std::shared_ptr<AbstractionBuilder>>("abs", "abstraction builder", "");

    parser.add_option<bool>("prune_dominated_by_parent",
                            "Prunes a state if it is dominated by its parent",
                            "false");

    parser.add_option<bool>("prune_dominated_by_initial_state",
                            "Prunes a state if it is dominated by the initial state",
                            "false");

    parser.add_option<int>("truncate_value",
                           "Assume -infinity if below minus this value",
                           "10");

    parser.add_option<int>("max_simulation_time",
                           "Maximum number of seconds spent in computing a single update of a simulation", "1800");

    parser.add_option<int>("min_simulation_time",
                           "Minimum number of seconds spent in computing a single update of a simulation", "1");

    parser.add_option<int>("num_labels_to_use_dominates_in",
                           "Use dominates_in for instances that have less than this amount of labels",
                           "0");

    parser.add_option<int>("max_total_time",
                           "Maximum number of seconds spent in computing all updates of a simulation", "1800");

    parser.add_option<int>("max_lts_size_to_compute_simulation",
                           "Avoid computing simulation on ltss that have more states than this number",
                           "1000000");

    parser.add_option<bool>("prune_successors",
                            "Prunes all siblings if any successor dominates the parent by enough margin",
                            "false");

    parser.add_option<bool>("use_quantified_dominance",
                            "Prune with respect to the quantified or the qualitative dominance",
                            "false");

    parser.add_option<bool>("trade_off_dominance",
                            "Compute dominatedBDD trading off positive and negative values",
                            "false");

    parser.add_option<bool>("only_positive_dominance",
                            "Compute dominatedBDDMaps only for positive values",
                            "false");

    options::Options opts = parser.parse();

    if (parser.dry_run()) {
        return nullptr;
    } else {
        return std::make_shared<SatisficingDominancePruning>(opts);
    }
}

static PluginTypePlugin<SatisficingDominancePruning> _plugin_type_satisficing_simulation("satisficing_simulation", "");
static Plugin<SatisficingDominancePruning> _plugin("satisficing_simulation", _parse);
//static Plugin<Heuristic> _plugin_h("satisficing_simulation", _parse_h);
}
