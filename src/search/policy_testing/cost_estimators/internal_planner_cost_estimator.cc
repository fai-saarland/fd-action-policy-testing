#include "internal_planner_cost_estimator.h"

#include "../../option_parser.h"
#include "../../plugin.h"
#include "../out_of_resource_exception.h"

namespace policy_testing {
static std::shared_ptr<AbstractTask> g_modified_task = nullptr;

static std::shared_ptr<AbstractTask>
_parse_modified_task(options::OptionParser &parser) {
    if (parser.dry_run()) {
        return nullptr;
    } else {
        assert(g_modified_task != nullptr);
        return g_modified_task;
    }
}

static Plugin<AbstractTask>
_plugin_modified_task("initial_state_modification", _parse_modified_task);

InternalPlannerPlanCostEstimator::InternalPlannerPlanCostEstimator(const options::Options &opts)
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
InternalPlannerPlanCostEstimator::add_options_to_parser(options::OptionParser &parser) {
    parser.add_enum_option<Configuration>(
        "conf",
        std::vector<std::string>(
            {"astar_lmcut", "lama_first", "lama_complete", "ehc_ff", "gbfs_ff", "lazy_gbfs_ff"}),
        "");
    parser.add_option<bool>("print_output", "", "false");
    parser.add_option<bool>("print_plan", "", "false");
    parser.add_option<int>("max_planner_time", "Maximal time to run internal planner.", "14400");
    parser.add_option<bool>("continue_after_time_out",
                            "Continue testing if internal planner oracle ran into a timeout.",
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
    std::shared_ptr<SearchEngine> engine = create(static_cast<int>(time_limit), start_state, goal_state);
    if (!engine) {
        if (!print_output_) {
            std::cout.clear();
            std::cerr.clear();
        }
        return false;
    }
    try{
        engine->search();
    } catch (const EngineInitException &) {
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
    if (print_plan_) {
        if (engine->found_solution()) {
            std::cout << "Plan found!\n";
            for (auto op_id : engine->get_plan()) {
                std::cout << get_task_proxy().get_operators()[op_id].get_name()
                          << "\n";
            }
            std::cout << std::flush;
        } else if (engine->get_status() != TIMEOUT) {
            std::cout << "No solution found." << std::endl;
        }
    }

    if (engine->found_solution()) {
        plan = engine->get_plan();
        assert(verify_plan(get_task(), start_state, plan));
        return true;
    } else if (engine->get_status() == TIMEOUT) {
        if (!continue_after_time_out || are_limits_reached()) {
            throw OutOfResourceException();
        } else {
            return false;
        }
    } else {
        return false;
    }
}

std::shared_ptr<SearchEngine>
InternalPlannerPlanCostEstimator::create(int max_time, const State &state, const State *goal_state) {
    g_modified_task = get_modified_task(get_task(), state, goal_state);

    const std::string max_time_str =
        max_time >= 0 ? (", max_time=" + std::to_string(max_time)) : "";

    switch (configuration_) {
    case Configuration::ASTAR_LMCUT:
    {
        const std::string argument =
            "astar(lmcut(transform=initial_state_modification()), "
            "transform=initial_state_modification()"
            + max_time_str + ")";
        options::Registry registry(*options::RawRegistry::instance());
        options::Predefinitions pre_definitions;
        options::OptionParser parser(argument, registry, pre_definitions, false);
        return parser.start_parsing<std::shared_ptr<SearchEngine>>();
    }

    case Configuration::LAMA_FIRST_ITERATION:
    {
        options::Registry registry(*options::RawRegistry::instance());
        options::Predefinitions pre_definitions;
        registry.handle_predefinition(
            "evaluator",
            "hlm=lmcount(lm_factory=lm_rhw(reasonable_orders=true), transform="
            "initial_state_modification(), pref=false)",
            pre_definitions,
            false);
        registry.handle_predefinition(
            "evaluator",
            "hff=ff(transform=initial_state_modification())",
            pre_definitions,
            false);
        auto argument =
            "lazy_greedy([hff,hlm], preferred=[hff,hlm], reopen_closed=false, "
            "transform=initial_state_modification()"
            + max_time_str + ")";

        options::OptionParser parser(
            argument, registry, pre_definitions, false);
        return parser.start_parsing<std::shared_ptr<SearchEngine>>();
    }

    case Configuration::LAMA_COMPLETE:
    {
        options::Registry registry(*options::RawRegistry::instance());
        options::Predefinitions pre_definitions;
        registry.handle_predefinition(
            "evaluator",
            "hlm=lmcount(lm_factory=lm_rhw(reasonable_orders=true), transform="
            "initial_state_modification(), pref=false)",
            pre_definitions,
            false);
        registry.handle_predefinition(
            "evaluator",
            "hff=ff(transform=initial_state_modification())",
            pre_definitions,
            false);
        auto search_string = [&max_time_str](
            const std::string &alg,
            const std::string &param = "") {
                return alg
                       + "([hff, hlm], preferred=[hff, hlm], "
                       "transform=initial_state_modification()"
                       + (param.empty() ? "" : ", ") + param + max_time_str + ")";
            };
        auto argument = "iterated([" + search_string("lazy_greedy")
            + search_string("lazy_wastar", "w=5")
            + search_string("lazy_wastar", "w=3")
            + search_string("lazy_wastar", "w=2")
            + search_string("lazy_wastar", "w=1")
            + "], repeat_last=true, continue_on_fail=true, "
            "transform=initial_state_modification()"
            + max_time_str + ")";
        options::OptionParser parser(
            argument, registry, pre_definitions, false);
        return parser.start_parsing<std::shared_ptr<SearchEngine>>();
    }

    case Configuration::EHC_FF:
    {
        const std::string argument =
            "ehc(h=ff(transform=initial_state_modification()), prevent_exit=true, "
            "transform=initial_state_modification()"
            + max_time_str + ")";
        options::Registry registry(*options::RawRegistry::instance());
        options::Predefinitions pre_definitions;
        options::OptionParser parser(
            argument, registry, pre_definitions, false);
        try{
            auto engine = parser.start_parsing<std::shared_ptr<SearchEngine>>();
            return engine;
        } catch (const EngineInitException &) {
            return nullptr;
        }
    }

    case Configuration::GBFS_FF:
    {
        const std::string argument = "eager_greedy(evals=[ff(transform=initial_state_modification())],"
            "transform=initial_state_modification(), max_expansions=1000" + max_time_str + ")";
        options::Registry registry(*options::RawRegistry::instance());
        options::Predefinitions pre_definitions;
        options::OptionParser parser(argument, registry, pre_definitions, false);
        return parser.start_parsing<std::shared_ptr<SearchEngine>>();
    }

    case Configuration::LAZY_GBFS_FF:
    {
        const std::string argument = "lazy_greedy(evals=[ff(transform=initial_state_modification())],"
            "transform=initial_state_modification(), max_expansions=1000" + max_time_str + ")";
        options::Registry registry(*options::RawRegistry::instance());
        options::Predefinitions pre_definitions;
        options::OptionParser parser(argument, registry, pre_definitions, false);
        return parser.start_parsing<std::shared_ptr<SearchEngine>>();
    }
    }

    return nullptr;
}

static Plugin<PlanCostEstimator> _plugin(
    "internal_planner_plan_cost_estimator",
    options::parse<PlanCostEstimator, InternalPlannerPlanCostEstimator>);
} // namespace policy_testing
