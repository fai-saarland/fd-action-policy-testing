#include "random_search.h"
#include "../option_parser.h"
#include "../plugin.h"
#include "../task_utils/successor_generator.h"
#include "../utils/logging.h"
#include "../utils/rng.h"

using namespace std;

namespace random_search {
RandomSearch::RandomSearch(const options::Options &opts)
    : SearchEngine(opts), rng(new utils::RandomNumberGenerator()) {}

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
        utils::g_log << "Goal state reached." << endl;
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
    int choice = (*rng)(applicable_ops.size());
    OperatorID op_id = applicable_ops[choice];
    OperatorProxy op = task_proxy.get_operators()[op_id];

    // calculate successor state.
    State succ_state = state_registry.get_successor_state(current_state, op);

    // check if the state was already visited
    if (visited_states.find(succ_state.get_id()) == visited_states.end()) {
        // if true, add to visited states and update
        visited_states[succ_state.get_id()] = op_id;
        current_state = succ_state;
        last_action_cost = op.get_cost();
        utils::g_log << "Moved to new state with cost " << last_action_cost << "." << endl;
    } else {
        // else, the state has already been visited. 
        // either explore another state or return the FAILED status
        utils::g_log << "State already visited." << endl;
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
    Options opts = parser.parse();

    shared_ptr<random_search::RandomSearch> engine;
    if (!parser.dry_run()) {
        engine = make_shared<random_search::RandomSearch>(opts);
    }

    return engine;
}

static Plugin<SearchEngine> _plugin("random_search", _parse);
}
