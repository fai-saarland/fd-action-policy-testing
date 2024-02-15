#include "internal_planner_cost_estimator.h"

#include "../../plugins/plugin.h"
#include "../out_of_resource_exception.h"
#include "../../heuristics/lm_cut_heuristic.h"
#include "../../search_algorithms/eager_search.h"
#include "../../search_algorithms/search_common.h"
#include "../../heuristics/ff_heuristic.h"
#include "../../search_algorithms/enforced_hill_climbing_search.h"
#include "../../pruning/null_pruning_method.h"

#include <memory>

namespace policy_testing {
static std::shared_ptr<AbstractTask> g_modified_task = nullptr;

InternalPlannerPlanCostEstimator::InternalPlannerPlanCostEstimator(const plugins::Options &opts)
    : PlanCostEstimator(),
      configuration_(opts.get<Configuration>("conf")),
      print_output_(opts.get<bool>("print_output")),
      print_plan_(opts.get<bool>("print_plan")),
      max_planner_time(opts.get<int>("max_planner_time")),
      continue_after_time_out(opts.get<bool>("continue_after_time_out")) {
}

InternalPlannerPlanCostEstimator::InternalPlannerPlanCostEstimator(TestingEnvironment *env, bool continue_after_timeout)
    : PlanCostEstimator(),
      configuration_(Configuration::ASTAR_LMCUT),
      print_output_(false),
      print_plan_(false),
      max_planner_time(14400),
      continue_after_time_out(continue_after_timeout) {
    connect_environment(env);
}

void
InternalPlannerPlanCostEstimator::add_options_to_feature(plugins::Feature &feature) {
    feature.add_option<Configuration>("conf", "search algorithm, possible choices: "
                                      "astar_lmcut, lama_first, lama_complete, ehc_ff, gbfs_ff, lazy_gbfs_ff");
    feature.add_option<bool>("print_output", "", "false");
    feature.add_option<bool>("print_plan", "", "false");
    feature.add_option<int>("max_planner_time", "Maximal time to run internal planner.", "14400");
    feature.add_option<bool>("continue_after_time_out",
                             "Continue testing if internal planner oracle ran into a timeout (or runs out of memory).",
                             "true");
}

int
InternalPlannerPlanCostEstimator::compute_value(const State &state) {
    std::vector<OperatorID> plan;
    if (run_planner(plan, state)) {
        return calculate_plan_cost(get_task(), plan);
    }
    // TODO one could return DEAD_END if the planner is guaranteed to be complete and has not been interrupted
    // return DEAD_END;
    return UNKNOWN;
}

int
InternalPlannerPlanCostEstimator::compute_trusted_value(const State &state, const State *goal_state) {
    std::vector<OperatorID> plan;
    if (run_planner(plan, state, goal_state)) {
        return calculate_plan_cost(get_task(), plan);
    }
    return DEAD_END;
}

int
InternalPlannerPlanCostEstimator::compute_trusted_value_with_cache(const State &start_state, const State *goal_state) {
    const StateID start_state_id = start_state.get_id();
    if (goal_state) {
        const StateID goal_state_id = goal_state->get_id();
        auto it = trusted_values_pairs_cache.find({start_state_id, goal_state_id});
        if (it != trusted_values_pairs_cache.end()) {
            return it->second;
        }
        int result = compute_trusted_value(start_state, goal_state);
        trusted_values_pairs_cache.insert({{start_state_id, goal_state_id}, result});
        return result;
    } else {
        auto it = trusted_values_cache.find(start_state_id);
        if (it != trusted_values_cache.end()) {
            return it->second;
        }
        int result = compute_trusted_value(start_state);
        trusted_values_cache.insert({start_state_id, result});
        return result;
    }
}

bool
InternalPlannerPlanCostEstimator::run_planner(std::vector<OperatorID> &plan, const State &start_state,
                                              const State *goal_state) {
    if (!print_output_) {
        std::cout.setstate(std::ios_base::failbit);
        std::cerr.setstate(std::ios_base::failbit);
    }
    const timestamp_t time_limit = std::min<timestamp_t>(get_remaining_time(), max_planner_time);
    std::shared_ptr<SearchAlgorithm> engine = create(static_cast<int>(time_limit), start_state, goal_state);
    if (!engine) {
        if (!print_output_) {
            std::cout.clear();
            std::cerr.clear();
        }
        return false;
    }
    try{
        engine->search();
    } catch (const enforced_hill_climbing_search::InitException &) {
        if (!print_output_) {
            std::cout.clear();
            std::cerr.clear();
        }
        return false;
    }
    if (!print_output_) {
        std::cout.clear();
        std::cerr.clear();
    }
    SearchStatus engine_exit_status = engine->get_status();
    if (print_plan_) {
        if (engine->found_solution()) {
            std::cout << "Plan found for state " << start_state << std::endl;
            for (auto op_id : engine->get_plan()) {
                std::cout << get_task_proxy().get_operators()[op_id].get_name()
                          << " " << op_id << "\n";
            }
            std::cout << std::flush;
        } else if (engine_exit_status != TIMEOUT && engine_exit_status != OOM) {
            std::cout << "No solution found." << std::endl;
        }
    }

    if (engine->found_solution()) {
        plan = engine->get_plan();
        assert(goal_state || verify_plan(get_task(), start_state, plan));
        return true;
    } else if (engine_exit_status == TIMEOUT || engine_exit_status == OOM) {
        if (!continue_after_time_out || are_limits_reached()) {
            throw OutOfResourceException();
        } else {
            utils::reestablish_extra_memory_padding(50);
            return false;
        }
    } else {
        return false;
    }
}

std::shared_ptr<SearchAlgorithm>
InternalPlannerPlanCostEstimator::create(double max_time, const State &state, const State *goal_state) {
    g_modified_task = get_modified_task(get_task(), state, goal_state);

    plugins::Options search_algorithm_opts;
    search_algorithm_opts.set("max_time", max_time);
    search_algorithm_opts.set("transform", g_modified_task);

    switch (configuration_) {
    case Configuration::ASTAR_LMCUT:
    {
        plugins::Options lmcut_opts;
        lmcut_opts.set("transform", g_modified_task);
        lmcut_opts.set("cache_estimates", true);
        lmcut_opts.set("verbosity", utils::Verbosity::SILENT);
        std::shared_ptr<Evaluator> lmcut = std::make_shared<lm_cut_heuristic::LandmarkCutHeuristic>(lmcut_opts);
        search_algorithm_opts.set("eval", lmcut);
        search_algorithm_opts.set("verbosity", utils::Verbosity::SILENT);
        auto [open, f_eval] = search_common::create_astar_open_list_factory_and_f_eval(search_algorithm_opts);
        search_algorithm_opts.set("open", open);
        search_algorithm_opts.set("f_eval", f_eval);
        search_algorithm_opts.set("reopen_closed", true);
        search_algorithm_opts.set("preferred", std::vector<std::shared_ptr<Evaluator>>());
        search_algorithm_opts.set("bound", std::numeric_limits<int>::max());
        search_algorithm_opts.set("cost_type", OperatorCost::NORMAL);
        plugins::Options pruning_options;
        pruning_options.set("verbosity", utils::Verbosity::SILENT);
        std::shared_ptr<PruningMethod> pruning_method = std::make_shared<null_pruning_method::NullPruningMethod>(pruning_options);
        search_algorithm_opts.set("pruning", pruning_method);
        return std::make_shared<eager_search::EagerSearch>(search_algorithm_opts);
    }

    case Configuration::EHC_FF:
    {
        plugins::Options ff_opts;
        ff_opts.set("transform", g_modified_task);
        ff_opts.set("cache_estimates", true);
        ff_opts.set("verbosity", utils::Verbosity::SILENT);
        std::shared_ptr<Evaluator> ff = std::make_shared<ff_heuristic::FFHeuristic>(ff_opts);
        search_algorithm_opts.set("h", ff);
        search_algorithm_opts.set("verbosity", utils::Verbosity::SILENT);
        search_algorithm_opts.set("prevent_exit", true);
        search_algorithm_opts.set("preferred_usage", enforced_hill_climbing_search::PreferredUsage::PRUNE_BY_PREFERRED);
        search_algorithm_opts.set("preferred", std::vector<std::shared_ptr<Evaluator>>());
        search_algorithm_opts.set("bound", std::numeric_limits<int>::max());
        search_algorithm_opts.set("cost_type", OperatorCost::NORMAL);
        return std::make_shared<enforced_hill_climbing_search::EnforcedHillClimbingSearch>(search_algorithm_opts);
    }
    }
    return nullptr;
}


class InternalPlannerPlanCostEstimatorFeature
    : public plugins::TypedFeature<PlanCostEstimator, InternalPlannerPlanCostEstimator> {
public:
    InternalPlannerPlanCostEstimatorFeature() : TypedFeature("internal_planner_plan_cost_estimator") {
        InternalPlannerPlanCostEstimator::add_options_to_feature(*this);
    }
};
static plugins::FeaturePlugin<InternalPlannerPlanCostEstimatorFeature> _plugin;

static plugins::TypedEnumPlugin<InternalPlannerPlanCostEstimator::Configuration> _enum_plugin({
        {"astar_lmcut", ""},
        {"ehc_ff", ""},
    });
} // namespace policy_testing
