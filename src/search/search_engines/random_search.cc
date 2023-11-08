#include "random_search.h"
#include "../option_parser.h"
#include "../plugin.h"
#include "../task_utils/successor_generator.h"
#include "../utils/logging.h"

using namespace std;

namespace random_search {
RandomSearch::RandomSearch(const options::Options &opts)
    : SearchEngine(opts) {
}

void RandomSearch::initialize() {
    utils::g_log << "Conducting random search." << endl;
    current_state = state_registry.get_initial_state();

    // Check if the initial state is a goal state.
    if (check_goal_and_set_plan(current_state)) {
        utils::g_log << "Initial state is a goal state." << endl;
    } else {
        utils::g_log << "Initial state is not a goal state. Starting random exploration." << endl;
        last_action_cost = 0;
    }
}

void RandomSearch::print_statistics() const {
    statistics.print_detailed_statistics();
    search_space.print_statistics();
}

SearchStatus RandomSearch::step() {
    if (check_goal_and_set_plan(state_registry.get_initial_state())) {
        return SOLVED;
    }
    // TODO: Performs one step of the search algorithm, i.e., it takes a
    // state and applies action (see successor_generator.generate_applicable_ops()).
    // This function returns FAILED if solution was not found, SOLVED if a
    // solution was found (in which case the plan has to be recorded), and
    // IN_PROGRESS if the search should continue.

    // TODO: For caching of selected actions, keep in mind that state IDs
    // are monotonically increasing 0, 1, ...

    utils::g_log << "Logging example" << endl;
    return IN_PROGRESS;
}


static shared_ptr<SearchEngine> _parse(OptionParser &parser) {
    parser.document_synopsis(
        "Random search",
        "TODO: Description");
    // Use parser.add_option() to add more options

    SearchEngine::add_pruning_option(parser);
    SearchEngine::add_options_to_parser(parser);
    Options opts = parser.parse();

    shared_ptr<random_search::RandomSearch> engine;
    if (!parser.dry_run()) {
        engine = make_shared<random_search::RandomSearch>(opts);
    }

    return engine;
}

static Plugin<SearchEngine> _plugin("random_search", _parse);
}
