#include "random_search.h"
#include "../option_parser.h"
#include "../plugin.h"
#include "../task_utils/successor_generator.h"
#include "../utils/logging.h"
#include "../utils/rng.h"

using namespace std;

namespace random_search {
RandomSearch::RandomSearch(const options::Options &opts)
    : SearchEngine(opts),
      rng(opts.get<int>("seed")),
      max_steps(opts.get<int>("max-steps")),
      num_steps(0) {
    utils::g_log << "Random Search: seed = " << opts.get<int>("seed") << endl;
    utils::g_log << "Random Search: max-steps = " << max_steps << endl;
}

void RandomSearch::initialize() {
    utils::g_log << "Conducting random search." << endl;
    current_state = state_registry.get_initial_state();
}

void RandomSearch::print_statistics() const {
    statistics.print_detailed_statistics();
    search_space.print_statistics();
}

SearchStatus RandomSearch::step() {
    // always checking for goal state first
    if (check_goal_and_set_plan(current_state)) {
        utils::g_log << "Goal state reached in " << num_steps << " steps" << endl;
        set_plan(plan);
        return SOLVED;
    }

    // generate all applicable operations
    vector<OperatorID> applicable_ops;
    successor_generator.generate_applicable_ops(current_state, applicable_ops);

    //  catch if there is no valid operation
    if (applicable_ops.empty()) {
        utils::g_log << "No applicable actions; search failed." << endl;
        return FAILED;
    }

    // now choose randomly one of the operations
    int choice = rng(applicable_ops.size());
    OperatorID op_id = applicable_ops[choice];
    OperatorProxy op = task_proxy.get_operators()[op_id];
    plan.push_back(op_id);

    // calculate successor state.
    current_state = state_registry.get_successor_state(current_state, op);
    statistics.inc_generated();

    ++num_steps;
    if (num_steps >= max_steps) {
        utils::g_log << "Reached maximal number steps " << max_steps << endl;
        return FAILED;
    }
    return IN_PROGRESS;
}

// TODO: Performs one step of the search algorithm, i.e., it takes a
// state and applies action (see successor_generator.generate_applicable_ops()).
// This function returns FAILED if solution was not found, SOLVED if a
// solution was found (in which case the plan has to be recorded), and
// IN_PROGRESS if the search should continue.

// TODO: For caching of selected actions, keep in mind that state IDs
// are monotonically increasing 0, 1, ...



static shared_ptr<SearchEngine> _parse(OptionParser &parser) {
    parser.document_synopsis(
        "Random search",
        "TODO: Description");
    // Use parser.add_option() to add more options

    SearchEngine::add_pruning_option(parser);
    SearchEngine::add_options_to_parser(parser);
    parser.add_option<int>("seed", "Random Seed", "1734");
    parser.add_option<int>("max-steps", "Maximum number of steps", "1000");
    Options opts = parser.parse();

    shared_ptr<random_search::RandomSearch> engine;
    if (!parser.dry_run()) {
        engine = make_shared<random_search::RandomSearch>(opts);
    }

    return engine;
}

static Plugin<SearchEngine> _plugin("random_search", _parse);
}
