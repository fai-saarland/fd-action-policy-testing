#include "asnet_benchmarking.h"

#include "../../option_parser.h"
#include "../../plugin.h"
#include "../../task_utils/successor_generator.h"
#include "../../task_utils/task_properties.h"

#include <iostream>
#include <vector>

namespace policy_testing {
ASNetBenchmarkingEngine::ASNetBenchmarkingEngine(const options::Options &opts)
    : SearchEngine(opts)
      , policy_(
          opts.get<std::string>("domain_pddl"),
          opts.get<std::string>("problem_pddl"),
          opts.get<std::string>("snapshot")) {
    std::cout << "ASNet initialization: " << utils::g_timer << std::endl;
}

void
ASNetBenchmarkingEngine::add_options_to_parser(options::OptionParser &parser) {
    parser.add_option<std::string>("domain_pddl", "Domain PDDL", "");
    parser.add_option<std::string>("problem_pddl", "Problem PDDL", "");
    parser.add_option<std::string>("snapshot", "Snapshot .pkl file", "");
    SearchEngine::add_options_to_parser(parser);
}

SearchStatus
ASNetBenchmarkingEngine::step() {
    double total = 0;
    utils::Timer eval;
    State state(state_registry.get_initial_state());
    Plan plan;
    std::vector<OperatorID> applicable;
    utils::HashSet<StateID> closed;
    while (!task_properties::is_goal_state(task_proxy, state)) {
        successor_generator.generate_applicable_ops(state, applicable);
        if (applicable.empty()) {
            std::cout << "terminal state!" << std::endl;
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
        if (!closed.insert(state.get_id()).second) {
            std::cout << "cycle: state " << state.get_id() << std::endl;
            return FAILED;
        }
        plan.push_back(op);
    }
    set_plan(plan);
    std::cout << "Solution found!" << std::endl;
    std::cout << "Total ASNet evaluation time: " << total << "s" << std::endl;
    return SOLVED;
}

static Plugin<SearchEngine>
_plugin("run_asnet", options::parse<SearchEngine, ASNetBenchmarkingEngine>);
} // namespace policy_testing
