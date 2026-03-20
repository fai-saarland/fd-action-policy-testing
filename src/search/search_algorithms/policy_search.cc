#include "policy_search.h"
#include "../plugins/plugin.h"

using namespace std;
using namespace policy_testing;

namespace policy_search {
PolicySearch::PolicySearch(const plugins::Options &opts)
    : SearchAlgorithm(opts),
      policy(opts.contains("policy") ? opts.get<std::shared_ptr<policy_testing::Policy>>("policy"): nullptr),
      current_state(state_registry.get_initial_state()),
      env(task, &state_registry) { // TODO: what is good practice here?
    // Establish the remote policy connection, if no other policy was specified.
    if (!policy) {
        if (RemotePolicy::connection_established()) {
            utils::g_log << "No additional policy specification found. "
                "Assuming global remote_policy with standard configuration." << std::endl;
            policy = RemotePolicy::get_global_default_policy();
            utils::g_log << "Attempting to connect environment." << std::endl;
            policy->connect_environment(&env);
        } else {
            std::cerr << "You need to provide a policy." << std::endl;
            utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
        }
    }

    policy->execute_get_plan_and_path(current_state, plan, path);
}

SearchStatus PolicySearch::step() {
    if (check_goal_and_set_plan(current_state)) {
        return SOLVED;
    }
    if (path.size() > path_index) {
        current_state = path[path_index];
        SearchNode node = search_space.get_node(current_state);
        if (parent_id == StateID::no_state) {
            node.open_initial();
        } else {
            SearchNode parent_node = search_space.get_node(parent_state);
            OperatorProxy last_op = task_proxy.get_operators()[plan[path_index - 1]];
            node.open(parent_node, last_op, get_adjusted_cost(last_op));
        }
        node.close();

        parent_state = current_state;
        parent_id = parent_state.get_id();
        statistics.inc_expanded();
        statistics.inc_evaluated_states();

        path_index++;
        return IN_PROGRESS;
    } else {
        return FAILED;
    }
    return FAILED;
}

void PolicySearch::print_statistics() const {
    statistics.print_detailed_statistics();
    search_space.print_statistics();
}

void add_options_to_feature(plugins::Feature &feature) {
    SearchAlgorithm::add_pruning_option(feature);
    SearchAlgorithm::add_options_to_feature(feature);
}
}
