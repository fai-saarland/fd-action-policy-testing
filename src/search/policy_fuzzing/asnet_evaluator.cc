#include "asnet_evaluator.h"

#include "../option_parser.h"
#include "../per_state_bitset.h"
#include "../plugin.h"
#include "../task_proxy.h"
#include "../task_utils/successor_generator.h"
#include "../task_utils/task_properties.h"
#include "../utils/timer.h"

#include <iostream>
#include <vector>

namespace policy_fuzzing {

ASNetEvaluatorEngine::ASNetEvaluatorEngine(const options::Options& opts)
    : SearchEngine(opts)
    , policy_(
          opts.get<std::string>("domain_pddl"),
          opts.get<std::string>("problem_pddl"),
          opts.get<std::string>("snapshot"))
{
    std::cout << "ASNet initialization: " << utils::g_timer << std::endl;
}

void
ASNetEvaluatorEngine::add_options_to_parser(options::OptionParser& parser)
{
    parser.add_option<std::string>("domain_pddl", "Domain PDDL", "");
    parser.add_option<std::string>("problem_pddl", "Problem PDDL", "");
    parser.add_option<std::string>("snapshot", "Snapshot .pkl file", "");
    SearchEngine::add_options_to_parser(parser);
}

SearchStatus
ASNetEvaluatorEngine::step()
{
    double total = 0;
    utils::Timer eval;
    State state(state_registry.get_initial_state());
    Plan plan;
    std::vector<OperatorID> applicable;
    PerStateBitset closed({ false });
    closed[state].set(0);
    while (!task_properties::is_goal_state(task_proxy, state)) {
        successor_generator.generate_applicable_ops(state, applicable);
        if (applicable.empty()) {
            std::cout << "terminal dead-end!" << std::endl;
            return FAILED;
        }
        std::cout << "Calling ASNet on state " << state.get_id() << std::flush;
        eval.reset();
        OperatorID op = policy_.apply_policy(state, applicable);
        std::cout << ": " << task_proxy.get_operators()[op].get_name() << " "
                  << eval << " [t=" << utils::g_timer << "]" << std::endl;
        total += eval();
        applicable.clear();
        state = state_registry.get_successor_state(
            state, task_proxy.get_operators()[op]);
        plan.push_back(op);
        auto b = closed[state];
        if (b.test(0)) {
            std::cout << "cycle: state " << state.get_id() << std::endl;
            return FAILED;
        }
        b.set(0);
    }
    set_plan(plan);
    std::cout << "Solution found!" << std::endl;
    std::cout << "Total ASNet evaluation time: " << total << "s" << std::endl;
    return SOLVED;
}

static Plugin<SearchEngine>
    _plugin("run_asnet", options::parse<SearchEngine, ASNetEvaluatorEngine>);

} // namespace policy_fuzzing
