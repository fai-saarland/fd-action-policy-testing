#include "abstraction_builder.h"

#include <memory>

#include "abstraction.h"
#include "dominance_relation.h"
#include "shrink_strategy.h"
#include "shrink_bisimulation.h"
#include "shrink_composite.h"
#include "shrink_own_labels.h"
#include "merge_strategy.h"
#include "variable_partition_finder.h"
#include "label_reducer.h"

namespace simulations {
void AbstractionBuilder::init_ldsim(bool unit_cost, OperatorCost cost_type,
                                    std::unique_ptr<LDSimulation> &ldSim) const {
    ldSim = std::make_unique<LDSimulation>(unit_cost, opts, cost_type);
}

void AbsBuilderComposite::build_abstraction(bool unit_cost, OperatorCost cost_type,
                                            std::unique_ptr<LDSimulation> &ldSim,
                                            std::vector<std::unique_ptr<Abstraction>> &abstractions) const {
    for (auto st: strategies) {
        st->build_abstraction(unit_cost, cost_type, ldSim, abstractions);
    }
}


void AbsBuilderPDB::build_abstraction(bool unit_cost, OperatorCost cost_type,
                                      std::unique_ptr<LDSimulation> &ldSim,
                                      std::vector<std::unique_ptr<Abstraction>> & /*abstractions*/) const {
    if (ldSim) {
        std::cerr << "Error: AbsBuilderPDB can only be used to initialize the abstractions" << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
    }

    init_ldsim(unit_cost, cost_type, ldSim);

    VariablePartitionGreedy v(limit_absstates_merge);
    ldSim->init_factored_systems(v.get_partition());
}


void AbsBuilderAtomic::build_abstraction(bool unit_cost, OperatorCost cost_type,
                                         std::unique_ptr<LDSimulation> &ldSim,
                                         std::vector<std::unique_ptr<Abstraction>> & /*abstractions*/) const {
    if (!ldSim) {
        init_ldsim(unit_cost, cost_type, ldSim);
        ldSim->init_atomic_abstractions();
    }
}



void AbsBuilderMasSimulation::build_abstraction(bool unit_cost, OperatorCost cost_type,
                                                std::unique_ptr<LDSimulation> &ldSim,
                                                std::vector<std::unique_ptr<Abstraction>> & /*abstractions*/) const {
    Abstraction::store_original_operators = store_original_operators;

    if (!ldSim) {
        init_ldsim(unit_cost, cost_type, ldSim);
    }

    int remaining_time = std::max(0, std::min<int>(limit_seconds_mas, limit_seconds_total - static_cast<int>(utils::g_timer())));


    ldSim->build_abstraction(merge_strategy.get(), limit_absstates_merge, min_limit_absstates_merge,
                             limit_transitions_merge, original_merge,
                             shrink_strategy.get(), forbid_lr,
                             remaining_time, limit_memory_kb_total,
                             intermediate_simulations, incremental_simulations,
                             simulation_type,
                             label_dominance_type,
                             switch_off_label_dominance,
                             complex_lts,
                             apply_subsumed_transitions_pruning, apply_label_dominance_reduction,
                             apply_simulation_shrinking, /*preserve_all_optimal_plans*/ false,
                             expensive_statistics);


    if (compute_final_simulation)
        ldSim->compute_final_simulation(simulation_type,
                                        label_dominance_type,
                                        switch_off_label_dominance,
                                        intermediate_simulations, complex_lts,
                                        apply_subsumed_transitions_pruning,
                                        apply_label_dominance_reduction, apply_simulation_shrinking,
                                        /*preserve_all_optimal_plans*/ false,
                                        dump);

    if (prune_dead_operators)
        ldSim->prune_dead_ops();
}


void AbsBuilderMAS::build_abstraction(bool unit_cost, OperatorCost cost_type,
                                      std::unique_ptr<LDSimulation> &ldSim,
                                      std::vector<std::unique_ptr<Abstraction>> &abstractions) const {
    Abstraction::store_original_operators = store_original_operators;

    for (int i = 0; i < num_abstractions; ++i) {
        int remaining_time = std::max(0, std::min<int>(limit_seconds_mas, limit_seconds_total - static_cast<int>(utils::g_timer())));
        if (remaining_time <= 0)
            break;

        if (restart) {
            ldSim->release_memory();
            std::unique_ptr<LDSimulation> tmpldSim;
            init_ldsim(unit_cost, cost_type, tmpldSim);
            tmpldSim->init_atomic_abstractions();

            tmpldSim->complete_heuristic(merge_strategy.get(), shrink_strategy.get(), shrink_after_merge,
                                         remaining_time, limit_memory_kb_total, prune_dead_operators,
                                         expensive_statistics, abstractions);
        } else {
            if (!ldSim) {
                init_ldsim(unit_cost, cost_type, ldSim);
                ldSim->init_atomic_abstractions();
            }

            ldSim->complete_heuristic(merge_strategy.get(), shrink_strategy.get(), shrink_after_merge,
                                      remaining_time, limit_memory_kb_total, prune_dead_operators,
                                      expensive_statistics, abstractions);
        }
    }
}


void AbsBuilderMasSimulation::dump_options() const {
    std::cout << "AbsBuilderMasSimulation" << std::endl;
    merge_strategy->dump_options();
    if (shrink_strategy)
        shrink_strategy->dump_options();
    else
        std::cout << " no shrinking" << std::endl;

    std::cout << "Expensive statistics: "
              << (expensive_statistics ? "enabled" : "disabled") << std::endl;

    if (expensive_statistics) {
        std::string dashes(79, '=');
        std::cerr << dashes << std::endl
                  << ("WARNING! You have enabled extra statistics for "
            "merge-and-shrink heuristics.\n"
            "These statistics require a lot of time and memory.\n"
            "When last tested (around revision 3011), enabling the "
            "extra statistics\nincreased heuristic generation time by "
            "76%. This figure may be significantly\nworse with more "
            "recent code or for particular domains and instances.\n"
            "You have been warned. Don't use this for benchmarking!")
                  << std::endl << dashes << std::endl;
    }
}

AbstractionBuilder::AbstractionBuilder(const plugins::Options &opts) :
    opts(opts), expensive_statistics(opts.get<bool>("expensive_statistics")),
    dump(opts.get<bool>("dump")),
    limit_seconds_total(opts.get<int>("limit_seconds_total")),
    limit_memory_kb_total(opts.get<int>("limit_memory_kb")) {
}

AbsBuilderAtomic::AbsBuilderAtomic(const plugins::Options &opts) :
    AbstractionBuilder(opts) {
}

AbsBuilderPDB::AbsBuilderPDB(const plugins::Options &opts) :
    AbstractionBuilder(opts),
    limit_absstates_merge(opts.get<int>("limit_absstates_merge")) {
}

AbsBuilderComposite::AbsBuilderComposite(const plugins::Options &opts) :
    AbstractionBuilder(opts),
    strategies(opts.get_list<AbstractionBuilder *>("strategies")) {
    if (strategies.empty()) {
        std::cerr << "strategies option of AbsBuilderComposite must not be empty" << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
    }
}


AbsBuilderMasSimulation::AbsBuilderMasSimulation(const plugins::Options &opts) :
    AbstractionBuilder(opts),
    simulation_type(opts.get<SimulationType>("simulation_type")),
    label_dominance_type(opts.get<LabelDominanceType>("label_dominance_type")),
    switch_off_label_dominance(opts.get<int>("switch_off_label_dominance")),
    apply_simulation_shrinking(opts.get<bool>("apply_simulation_shrinking")),
    apply_subsumed_transitions_pruning(opts.get<bool>("apply_subsumed_transitions_pruning")),
    apply_label_dominance_reduction(opts.get<bool>("apply_label_dominance_reduction")),
    prune_dead_operators(opts.get<bool>("prune_dead_operators")),
    store_original_operators(opts.get<bool>("store_original_operators")),
    complex_lts(opts.get<bool>("complex_lts")),
    merge_strategy(opts.contains("merge_strategy") ? opts.get<std::shared_ptr<MergeStrategy>>("merge_strategy") : nullptr),
    original_merge(opts.get<bool>("original_merge")),
    limit_absstates_merge(opts.get<int>("limit_merge")),
    min_limit_absstates_merge(opts.get<int>("min_limit_merge")),
    limit_transitions_merge(opts.get<int>("limit_transitions_merge")),
    intermediate_simulations(opts.get<bool>("intermediate_simulations")),
    incremental_simulations(opts.get<bool>("incremental_simulations")),
    compute_final_simulation(opts.get<bool>("compute_final_simulation")),
    forbid_lr(opts.get<bool>("forbid_lr")),
    shrink_strategy(opts.contains("shrink_strategy") ?  opts.get<std::shared_ptr<ShrinkStrategy>>("shrink_strategy") : nullptr),
    shrink_after_merge(opts.get<bool>("shrink_after_merge")),
    limit_seconds_mas(opts.get<int>("limit_seconds")) {
    if (opts.get<bool>("incremental_pruning")) {
        apply_subsumed_transitions_pruning = true;
        prune_dead_operators = true;
        store_original_operators = true;
        intermediate_simulations = true;
        incremental_simulations = true;
    }

    if (incremental_simulations && !intermediate_simulations) {
        std::cerr << "Error: To use incremental calculation of simulations, intermediate simulations must be used!"
                  << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
    }

    if (!prune_dead_operators && Abstraction::store_original_operators) {
        std::cerr << "Error: Why do you want to store operators if you don't prune them?" << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
    }
}


AbsBuilderMAS::AbsBuilderMAS(const plugins::Options &opts) :
    AbstractionBuilder(opts),
    merge_strategy(opts.contains("merge_strategy") ? opts.get<std::shared_ptr<MergeStrategy>>("merge_strategy") : nullptr),
    shrink_strategy(opts.contains("shrink_strategy") ?  opts.get<std::shared_ptr<ShrinkStrategy>>("shrink_strategy") : nullptr),
    shrink_after_merge(opts.get<bool>("shrink_after_merge")),
    limit_seconds_mas(opts.get<int>("limit_seconds")),
    prune_dead_operators(opts.get<bool>("prune_dead_operators")),
    store_original_operators(opts.get<bool>("store_original_operators")),
    restart(opts.get<bool>("restart")),
    num_abstractions(opts.get<int>("num_abstractions")) {
}


AbsBuilderDefault::AbsBuilderDefault(const plugins::Options &opts) :
    AbstractionBuilder(opts),
    merge_strategy(opts.contains("merge_strategy") ? opts.get<std::shared_ptr<MergeStrategy>>("merge_strategy") : nullptr),
    original_merge(opts.get<bool>("original_merge")),
    limit_absstates_merge(opts.get<int>("limit_merge")),
    min_limit_absstates_merge(opts.get<int>("min_limit_merge")),
    limit_transitions_merge(opts.get<int>("limit_transitions_merge")),
    limit_absstates_shrink(opts.get<int>("limit_shrink")),

    limit_seconds_mas(opts.get<int>("limit_seconds")),
    num_abstractions(opts.get<int>("num_abstractions")),
    switch_off_label_dominance(opts.get<int>("switch_off_label_dominance")) {
}


void AbstractionBuilder::add_options_to_feature(plugins::Feature &feature) {
    feature.add_option<LabelReducer::LabelReductionMethod>(
        "label_reduction_method",
        "label reduction method: "
        "none: no label reduction will be performed "
        "old: emulate the label reduction as described in the "
        "IJCAI 2011 paper by Nissim, Hoffmann and Helmert."
        "two_abstractions: compute the 'combinable relation' "
        "for labels only for the two abstractions that will "
        "be merged next and reduce labels."
        "all_abstractions: compute the 'combinable relation' "
        "for labels once for every abstraction and reduce "
        "labels."
        "all_abstractions_with_fixpoint: keep computing the "
        "'combinable relation' for labels iteratively for all "
        "abstractions until no more labels can be reduced.",
        "ALL_ABSTRACTIONS_WITH_FIXPOINT");

    feature.add_option<LabelReducer::LabelReductionSystemOrder>("label_reduction_system_order",
                                                                "order of transition systems for the label reduction methods "
                                                                "that iterate over the set of all abstractions. only useful "
                                                                "for the choices all_abstractions and all_abstractions_with_fixpoint "
                                                                "for the option label_reduction_method.",
                                                                "RANDOM");

    feature.add_option<int>("label_reduction_max_time",
                            "limit the number of seconds for label reduction",
                            "60");

    feature.add_option<bool>("expensive_statistics",
                             "show statistics on \"unique unlabeled edges\" (WARNING: "
                             "these are *very* slow, i.e. too expensive to show by default "
                             "(in terms of time and memory). When this is used, the planner "
                             "prints a big warning on stderr with information on the performance impact. "
                             "Don't use when benchmarking!)",
                             "false");

    feature.add_option<bool>("dump", "Dump relation", "false");

    feature.add_option<int>("limit_seconds_total",
                            "limit the number of seconds for building the merge and shrink abstractions"
                            "By default: 1400, reserving ~100 seconds for the preprocessor and ~300 for search  ",
                            "1400");

    feature.add_option<int>("limit_memory_kb",
                            "limit the memory for building the merge and shrink abstractions"
                            "By default:  ",
                            "4000000");
}


void AbsBuilderDefault::build_abstraction(bool unit_cost, OperatorCost cost_type,
                                          std::unique_ptr<LDSimulation> &ldSim_,
                                          std::vector<std::unique_ptr<Abstraction>> &abstractions) const {
    Abstraction::store_original_operators = true;

    bool preserve_all_optimal_plans = (ldSim_ != nullptr);
    LDSimulation *ldSim = nullptr;
    std::unique_ptr<LDSimulation> tmpldSim;

    if (!ldSim_) {
        init_ldsim(unit_cost, cost_type, ldSim_);
        ldSim = ldSim_.get();
    } else {
        ldSim_->release_memory();
        init_ldsim(unit_cost, cost_type, tmpldSim);
        ldSim = tmpldSim.get();
    }


    int remaining_time = std::max(0, std::min<int>(limit_seconds_mas, limit_seconds_total - static_cast<int>(utils::g_timer())));


    // 1) Incremental simulations without shrinking or label reduction

    std::cout << "1) Incremental simulations without shrinking or label reduction. Max states: "
              << limit_absstates_merge << " transitions: " << limit_transitions_merge << " Min states: "
              << min_limit_absstates_merge << std::endl;

    ldSim->build_abstraction(merge_strategy.get(), limit_absstates_merge, min_limit_absstates_merge,
                             limit_transitions_merge, /*original_merge*/ true,
                             nullptr, /*forbid_lr*/ true,
                             remaining_time, limit_memory_kb_total,
                             /*intermediate_simulations */ true, /*incremental_simulations*/ true,
                             SimulationType::SIMPLE,
                             LabelDominanceType::NORMAL,
                             switch_off_label_dominance,
                             /*complex_lts*/ false,
                             /*apply_subsumed_transitions_pruning*/ true, /*apply_label_dominance_reduction*/ false,
                             /*apply_simulation_shrinking*/ false, preserve_all_optimal_plans, expensive_statistics);


    ldSim->compute_final_simulation(SimulationType::SIMPLE,
                                    LabelDominanceType::NORMAL,
                                    switch_off_label_dominance,
                                    true, false,
                                    true,
                                    false, false, preserve_all_optimal_plans,
                                    /*dump*/ false);

    ldSim->prune_dead_ops();

    // 2) Incremental simulations with shrinking and label reduction
    std::cout << "2) Incremental simulations with shrinking and label reduction " << std::endl;
    std::shared_ptr<ShrinkStrategy> bisim(ShrinkBisimulation::create_default(true));

    remaining_time = std::max(0, std::min<int>(limit_seconds_mas, limit_seconds_total - static_cast<int>(utils::g_timer())));

    ldSim->build_abstraction(merge_strategy.get(), limit_absstates_merge, min_limit_absstates_merge,
                             limit_transitions_merge, original_merge,
                             bisim.get(), /*forbid_lr*/ false,
                             remaining_time, limit_memory_kb_total,
                             /*intermediate_simulations */ true, /*incremental_simulations*/ true,
                             SimulationType::SIMPLE,
                             LabelDominanceType::NORMAL,
                             switch_off_label_dominance,
                             /*complex_lts*/ false,
                             /*apply_subsumed_transitions_pruning*/ true, /*apply_label_dominance_reduction*/ false,
                             /*apply_simulation_shrinking*/ false, preserve_all_optimal_plans, expensive_statistics);

    ldSim->compute_final_simulation(SimulationType::SIMPLE,
                                    LabelDominanceType::NORMAL,
                                    switch_off_label_dominance,
                                    true, false,
                                    true,
                                    false, false, preserve_all_optimal_plans,
                                    dump);

    ldSim->prune_dead_ops();

    std::cout << "3) Complete abstractions" << std::endl;

    // 3) Complete abstractions
    // TODO (Jan) check switch to shared_ptr
    std::shared_ptr<ShrinkStrategy> bisim_limit(limit_absstates_shrink == 0 ?
                                                ShrinkBisimulation::create_default(true) :
                                                ShrinkBisimulation::create_default(true, limit_absstates_shrink));

    std::shared_ptr<ShrinkStrategy> own(ShrinkOwnLabels::create_default());
    std::vector<std::shared_ptr<ShrinkStrategy>> strategies;
    strategies.push_back(own);
    strategies.push_back(bisim_limit);
    std::shared_ptr<ShrinkStrategy> shrink_combined(ShrinkComposite::create_default(strategies));

    Abstraction::store_original_operators = false;

    for (int i = 0; i < num_abstractions; ++i) {
        int current_remaining_time = std::max(0, std::min<int>(limit_seconds_mas,
                                                               limit_seconds_total - static_cast<int>(utils::g_timer())));
        if (current_remaining_time <= 0)
            break;

        ldSim->complete_heuristic(merge_strategy.get(), shrink_combined.get(), /*shrink_after_merge*/ false,
                                  current_remaining_time, limit_memory_kb_total, /*prune_dead_operators*/ true,
                                  expensive_statistics, abstractions);
    }
}


static class AbstractionBuilderPlugin : public plugins::TypedCategoryPlugin<AbstractionBuilder> {
public:
    AbstractionBuilderPlugin() : TypedCategoryPlugin("AbstractionBuilder") {
        document_synopsis(
            "This page describes the different abstraction builders."
            );
    }
}
_category_plugin;


class AbsBuilderDefaultFeature : public plugins::TypedFeature<AbstractionBuilder, AbsBuilderDefault> {
public:
    AbsBuilderDefaultFeature() : TypedFeature("builder") {
        AbstractionBuilder::add_options_to_feature(*this);

        add_option<int>("limit_seconds",
                        "limit the number of seconds for each iteration. By default: 300 ",
                        "300");

        add_option<std::shared_ptr<MergeStrategy>>(
            "merge_strategy",
            "merge strategy; choose between merge_linear and merge_dfp",
            plugins::ArgumentInfo::NO_DEFAULT);

        add_option<int>("num_abstractions",
                        "how many abstractions should be generated",
                        "1");

        add_option<int>("limit_merge",
                        "limit on the number of abstract states after the merge. By default: 100000",
                        "100000");

        add_option<int>("min_limit_merge",
                        "minimum limit on the number of abstract states after the merge to apply transitions merge"
                        "By default: 0",
                        "0");

        add_option<int>("limit_shrink",
                        "limit on the number of abstract states for shrinking",
                        "100000");

        add_option<bool>("original_merge",
                         "Whether it continues merging variables after the next recommended merge has exceeded size",
                         "false");

        add_option<int>("limit_transitions_merge",
                        "limit on the number of transitions after the merge",
                        "100000");

        add_option<int>("switch_off_label_dominance",
                        "disables label dominance if there are too many labels"
                        "By default: 200, to avoid memory errors",
                        "200");
    }
};
static plugins::FeaturePlugin<AbsBuilderDefaultFeature> _default_plugin;


class AbsBuilderPDBFeature : public plugins::TypedFeature<AbstractionBuilder, AbsBuilderPDB> {
public:
    AbsBuilderPDBFeature() : TypedFeature("builder_pdb") {
        AbstractionBuilder::add_options_to_feature(*this);
        add_option<int>("limit_absstates_merge", "maximum number of states", "10000");
    }
};
static plugins::FeaturePlugin<AbsBuilderPDBFeature> _pdb_plugin;


class AbsBuilderMASFeature : public plugins::TypedFeature<AbstractionBuilder, AbsBuilderMAS> {
public:
    AbsBuilderMASFeature() : TypedFeature("builder_mas") {
        AbstractionBuilder::add_options_to_feature(*this);
        add_option<int>("limit_seconds",
                        "limit the number of seconds for each iteration. By default: 300 ",
                        "300");

        add_option<std::shared_ptr<MergeStrategy>>(
            "merge_strategy",
            "merge strategy; choose between merge_linear and merge_dfp",
            plugins::ArgumentInfo::NO_DEFAULT);

        add_option<bool>("shrink_after_merge",
                         "If true, performs the shrinking after merge instead of before",
                         "false");

        add_option<std::shared_ptr<ShrinkStrategy>>("shrink_strategy",
                                                    "shrink strategy; ",
                                                    plugins::ArgumentInfo::NO_DEFAULT);

        add_option<bool>("restart",
                         "If true, starts from atomic abstraction heuristics",
                         "false");

        add_option<int>("num_abstractions",
                        "how many abstractions should be generated",
                        "1");

        add_option<bool>("store_original_operators",
                         "Store the original operators for each transition in an abstraction",
                         "false");

        add_option<bool>("prune_dead_operators",
                         "Prune all operators that are dead in some abstraction. Note: not yet implemented; so far, only the number of dead operators is returned!",
                         "true");
    }
};
static plugins::FeaturePlugin<AbsBuilderMASFeature> _mas_plugin;


class AbsBuilderMasSimulationFeature : public plugins::TypedFeature<AbstractionBuilder, AbsBuilderMasSimulation> {
public:
    AbsBuilderMasSimulationFeature() : TypedFeature("builder_massim") {
        AbstractionBuilder::add_options_to_feature(*this);
        add_option<int>("limit_seconds",
                        "limit the number of seconds for each iteration"
                        "By default: 300 ",
                        "300");

        add_option<int>("limit_merge",
                        "limit on the number of abstract states after the merge"
                        "By default: 1, does not perform any merge",
                        "50000");

        add_option<int>("min_limit_merge",
                        "minimum limit on the number of abstract states after the merge"
                        "By default: 1, does not perform any merge",
                        "0");

        add_option<bool>("original_merge",
                         "Whether it continues merging variables after the next recommended merge has exceeded size",
                         "false");

        add_option<int>("limit_transitions_merge",
                        "limit on the number of transitions after the merge"
                        "By default: 0: no limit at all",
                        "50000");

        add_option<bool>("intermediate_simulations",
                         "Compute intermediate simulations and use them for shrinking",
                         "false");

        add_option<bool>("compute_final_simulation",
                         "Compute intermediate simulations and use them for shrinking",
                         "true");

        add_option<bool>("incremental_simulations",
                         "Compute incremental simulations and use them for shrinking",
                         "false");

        add_option<std::shared_ptr<MergeStrategy>>(
            "merge_strategy",
            "merge strategy; choose between merge_linear and merge_dfp",
            plugins::ArgumentInfo::NO_DEFAULT);

        add_option<bool>("complex_lts",
                         "Use the complex method for LTS representation",
                         "false");

        add_option<bool>("apply_simulation_shrinking",
                         "Perform simulation shrinking",
                         "false");

        add_option<bool>("apply_subsumed_transitions_pruning",
                         "Perform pruning of subsumed transitions, based on simulation shrinking. Note: can only be used if simulation shrinking is applied!",
                         "false");

        add_option<bool>("apply_label_dominance_reduction",
                         "Perform label reduction based on found label dominances",
                         "false");

        add_option<bool>("prune_dead_operators",
                         "Prune all operators that are dead in some abstraction. Note: not yet implemented; so far, only the number of dead operators is returned!",
                         "true");

        add_option<bool>("forbid_lr",
                         "Disable lr from the first part",
                         "false");

        add_option<bool>("store_original_operators",
                         "Store the original operators for each transition in an abstraction",
                         "false");

        add_option<bool>("shrink_after_merge",
                         "If true, performs the shrinking after merge instead of before",
                         "false");

        add_option<bool>("incremental_pruning",
                         "Sets to true apply_subsumed_transitions_pruning, prune_dead_operators, store_original_operators, intermediate_simulations, and incremental_simulations",
                         "false");

        add_option<std::shared_ptr<ShrinkStrategy>>("shrink_strategy",
                                                    "shrink strategy; ",
                                                    plugins::ArgumentInfo::NO_DEFAULT);

        add_option<SimulationType>("simulation_type",
                                   "type of simulation implementation: NONE, SIMPLE or COMPLEX .",
                                   "SIMPLE");

        add_option<LabelDominanceType>("label_dominance_type",
                                       "type of simulation implementation: NONE, NOOP or NORMAL.",
                                       "NORMAL");

        add_option<int>("switch_off_label_dominance",
                        "disables label dominance if there are too many labels"
                        "By default: 1000, to avoid memory errors",
                        "200");
    }
};
static plugins::FeaturePlugin<AbsBuilderMasSimulationFeature> _massim_plugin;

class AbsBuilderAtomicFeature : public plugins::TypedFeature<AbstractionBuilder, AbsBuilderAtomic> {
public:
    AbsBuilderAtomicFeature() : TypedFeature("builder_atomic") {
        AbstractionBuilder::add_options_to_feature(*this);
    }
};
static plugins::FeaturePlugin<AbsBuilderAtomicFeature> _atomic_plugin;

class AbsBuilderCompositeFeature : public plugins::TypedFeature<AbstractionBuilder, AbsBuilderComposite> {
public:
    AbsBuilderCompositeFeature() : TypedFeature("builder_composite") {
        AbstractionBuilder::add_options_to_feature(*this);
        add_list_option<std::shared_ptr<AbstractionBuilder>>("strategies");
    }
};
static plugins::FeaturePlugin<AbsBuilderCompositeFeature> _composite_plugin;
}
