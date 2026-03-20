#include "mult_policy_search.h"
#include "../plugins/plugin.h"
#include "../task_utils/task_properties.h"
#include "../evaluation_context.h"

using namespace std;
using namespace policy_testing;

namespace mult_policy_search {
MultPolicySearch::MultPolicySearch(const plugins::Options &opts)
    : SearchAlgorithm(opts),
      tree_search(opts.get<bool>("tree_search")),
      along_path(opts.get<bool>("along_path")),
      fault_detection(opts.get<bool>("fault_detection")),
      steps_per_cycle(opts.get<int>("step_depth")),
      number_of_policies(opts.get<int>("num_policies")),
      prefer_solved(opts.get<bool>("prefer_solved")),
      heuristic(opts.get<std::shared_ptr<Evaluator>>("h")),
      remote_policy(opts.contains("policy") ? opts.get<std::shared_ptr<policy_testing::RemotePolicy>>("policy"): nullptr),
      initial_state(state_registry.get_initial_state()),
      search_state(state_registry.get_initial_state()),
      env(task, &state_registry) { // TODO: what is good practice here?
    // DEFAULT SETTING: 5 policies, step_depth 5, tree_search off, along path off, heuristic lmcut

    // Establish the remote policy connection, if no other policy was specified.
    if (!remote_policy) {
        if (RemotePolicy::connection_established()) {
            utils::g_log << "No additional policy specification found. "
                "Assuming global remote_policy with standard configuration." << std::endl;
            remote_policy = RemotePolicy::get_global_default_policy();
            utils::g_log << "Attempting to connect environment." << std::endl;
            remote_policy->connect_environment(&env);
            utils::g_log << "Successfully connected environment." << std::endl;
        } else {
            std::cerr << "You need to provide a policy." << std::endl;
            utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
        }
    }

    if (opts.get<bool>("along_path")) {
        std::cout << "Along path configured." << std::endl;
    }
    if (tree_search && fault_detection) {
        std::cerr << "Not yet supported." << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
    }
    if (remote_policy->get_num_models() <= number_of_policies) {
        std::cerr << "Remote policy does not contain enough policies for the current configuration of " << number_of_policies << " policy models." << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
    }
    run_extended_policy_search(search_plan, search_path);
}

/**
 * How to order the improvement approaches?
 * - First run the portfolio -> Get best costs
 * - Use information to choose along path policy (does this help? -> test)
 *
 * - Further TODO: (add assertions)(integrate bound keeping)
 */
void MultPolicySearch::run_extended_policy_search(std::vector<OperatorID> &plan, std::vector<State> &path) {
    std::vector<int> pol_ranking;
    // Run multiple policies from initial state
    // TODO: change this to enum
    if (tree_search) {
        pol_ranking = imp_tree_search(initial_state, plan, path);
    } else if (fault_detection) {
        pol_ranking = imp_fault_search(plan, path);
    } else {
        pol_ranking = imp_mult_search(initial_state, plan, path);
    }

    // Run (best) policies along main's path
    if (along_path) {
        imp_along_path(plan, path, pol_ranking);
    }

#ifndef NDEBUG
    assert(verify_plan(task, initial_state, plan));
    assert(remote_policy->get_num_models() >= number_of_policies);
#endif
}

/**
 * Run multiple (i.e. all available) policies in the initial state. Take the best plan and path according to cost.
 * No further plan refinement here, just gets the best possible with "forward execution".
 *
 * @returns Ranking of policies
 */
std::vector<int> MultPolicySearch::imp_mult_search(State start_state, std::vector<OperatorID> &plan, std::vector<State> &path) {
    int best_id = 0;
    PolicyCost best_cost = -1;

    std::vector<std::pair<int, int>> cost_ranking;

    for (int i = 0; i < number_of_policies; ++i) {
        PolicyCost cost = remote_policy->compute_policy_cost(start_state, i);
        if (cost != Policy::UNSOLVED && cost != Policy::UNKNOWN) {
            cost_ranking.emplace_back(cost, i);
            log_mult_pol() << "Policy " << i << " found plan of cost " << cost << std::endl;
        } else {
            cost_ranking.emplace_back(std::numeric_limits<int>::max(), i);
        }

        // std::cout << "Trying to find new best ID (" << i << "," << cost << "). Current is " << best_id << " with cost " << best_cost << std::endl;
        if (cost != Policy::UNSOLVED && cost != Policy::UNKNOWN && (cost < best_cost || best_cost == -1)) {
            // std::cout << "Found new best ID " << best_id << " with cost " << cost << std::endl;
            best_cost = cost;
            best_id = i;

            if (prefer_solved) {
                break;
            }
        }
    }
    // std::cout << "Best ID is: " << best_id << " with cost " << best_cost << std::endl;
    // std::cout << "Plan length is " << plan.size() << std::endl;
    remote_policy->execute_get_transitions_and_path(start_state, plan, path, best_id); // Could still get stuck

    std::sort(cost_ranking.begin(), cost_ranking.end());
    std::vector<int> policy_ranking;
    for (auto pair : cost_ranking) {
        policy_ranking.push_back(pair.second);
    }

    // std::cout << "Cost Ranking: ";
    // for (auto [cost, id] : cost_ranking) {
    //     std::cout << "(" << cost << "," << id << ")";
    // }
    // std::cout << std::endl;

    // std::cout << "Policy Ranking: ";
    // for (int id : policy_ranking) {
    //     std::cout << " " << id;
    // }
    // std::cout << std::endl;

    return policy_ranking;
}

/**
 * Run multiple (i.e. all available) policies in the initial state up to some depth.
 * Then, take a heuristic estimate, which policy performed best, and continue from there.
 * No further plan refinement here, just gets the best possible with "forward execution".
 * If all policies get stuck (i.e. loop or cannot apply operators) or one policy reaches the goal, search ends.
 *
 * @returns Ranking of policies (TODO)
 */
std::vector<int> MultPolicySearch::imp_tree_search(State start_state, std::vector<OperatorID> &plan, std::vector<State> &path) {
    State step_state = start_state;
    // std::vector<bool> search_complete(number_of_policies);
    int solution_found = -1;
    bool search_stuck = false;

    // std::cout << "Tree init state" << std::endl;

    // while (std::find(search_complete.begin(), search_complete.end(), false) != search_complete.end()) { // TODO: How long should this be done??
    while (solution_found == -1 && !search_stuck) { // TODO: How long should this be done??
        std::vector<std::pair<PolicyCost, State>> search_step; // Could be used for backtracking
        int num_stuck = 0;

        // std::cout << "In while" << std::endl;

        // Run each policy up to a certain depth
        for (int idx = 0; idx < number_of_policies; ++idx) {
            // std::cout << "Policy index loop" << std::endl;

            std::vector<OperatorID> sub_plan;
            std::vector<State> sub_path;

            // std::cout << "Before running with index " << idx << "; Num of policies: " << number_of_policies << "Number in remote_policy: " << remote_policy->get_num_models() << std::endl;
            Policy::RunResult run_result = remote_policy->execute_get_transitions_and_path(step_state, sub_plan, sub_path, idx, -1, steps_per_cycle); // Execute for 'steps_per_cycle' steps
            // std::cout << "After running plan" << std::endl;

            // If cut, last state is missing -> cut plan by one
            if (!run_result.complete) {
                if (!sub_plan.empty()) {
                    sub_plan.pop_back();
                }
            }

            PolicyCost cost = calculate_plan_cost(sub_plan, task_proxy);
            State end_state = sub_path.size() != 0 ? sub_path.back() : step_state;

            // std::cout << "Got end state for policy " << idx << std::endl;

            if (!run_result.complete) {
                // Run terminated due to STEP LIMIT
                search_step.emplace_back(cost, end_state);
                // std::cout << "Run not complete" << std::endl;
                continue;
            }
            if (run_result.solves_state) {
                // If last state in path is GOAL
                search_step.emplace_back(cost, end_state);
                solution_found = idx;
                // std::cout << "Run solves state; idx " << solution_found << std::endl;
                continue;
            }
            // Run LOOPS or no OP applicable
            search_step.emplace_back(-1, end_state);
            ++num_stuck;
            // std::cout << "Run got stuck (loop or no OP)" << std::endl;
        }
        search_stuck = (num_stuck == number_of_policies);

        // std::cout << "Is search stuck? " << search_stuck << std::endl;

        // Determine heuristically best run
        std::pair<PolicyCost, PolicyCost> best_cost_total_pol = std::make_pair(-1, -1); // First entry total, second is policy cost only
        std::pair<State, State> chosen_state_total_pol = std::make_pair(start_state, start_state);
        std::pair<int, int> best_index_tot_pol = std::make_pair(-1, -1); // First entry total, second is by policy cost only
        for (int i = 0; i < search_step.size(); ++i) {
            // std::cout << "Best heuristic estimate deduction" << std::endl;

            auto [run_cost, end_state] = search_step[i];

            if (run_cost == -1) {
                continue;
            }

            EvaluationContext context(end_state);
            EvaluationResult heuristic_estimate = heuristic->compute_result(context);
            if (heuristic_estimate.is_infinite()) {
                if (heuristic->dead_ends_are_reliable()) {
                    // DEAD END
                    continue;
                }

                if (run_cost < best_cost_total_pol.second || best_cost_total_pol.second == -1) {
                    // Update for run_cost (in case heuristic always INF)
                    best_cost_total_pol.second = run_cost;
                    best_index_tot_pol.second = i;
                    chosen_state_total_pol.second = end_state;
                }
            } else {
                int total_cost = heuristic_estimate.get_evaluator_value() + run_cost; // run_cost CANNOT be -1, as caught above
                if (total_cost < best_cost_total_pol.first || best_cost_total_pol.first == -1) {
                    // Update for run_cost + heuristic
                    best_cost_total_pol.first = total_cost;
                    best_index_tot_pol.first = i;
                    chosen_state_total_pol.first = end_state;
                }
            }

            // std::cout << "End of best heuristic estimate deduction" << std::endl;
        }

        // std::cout << "Before determining best state" << std::endl;

        // std::cout << "Chosen state options: " << chosen_state_total_pol.first.get_id() << "," << chosen_state_total_pol.second.get_id() << std::endl;

        std::vector<OperatorID> best_sub_plan;
        std::vector<State> best_sub_path;
        State chosen_state;
        int best_index;
        if (solution_found != -1) {
            best_index = solution_found;
            chosen_state = search_step[best_index].second;
        } else if (best_index_tot_pol.first != -1) {
            chosen_state = chosen_state_total_pol.first;
            best_index = best_index_tot_pol.first;
        } else if (best_index_tot_pol.second != -1) {
            chosen_state = chosen_state_total_pol.second;
            best_index = best_index_tot_pol.second;
        } else {
            // No reasonable result found, e.g. if heuristic detects DEAD END for all paths.
            std::cout << "No solution found in tree search, aborting." << std::endl;
            return std::vector<int>(0);
        }

        // std::cout << "After determining best state" << std::endl;

        Policy::RunResult run_res = remote_policy->execute_get_transitions_and_path(step_state, best_sub_plan, best_sub_path, best_index, -1, steps_per_cycle);
        if (!run_res.complete) {
            best_sub_plan.pop_back();
        }
        for (OperatorID op : best_sub_plan) {
            plan.emplace_back(op);
        }

        // // PRINTOUT***PRINTOUT***PRINTOUT
        // //
        // OperatorsProxy ops_proxy = task_proxy.get_operators();
        // std::cout << "Current plan: ";
        // for (auto op : plan) {
        //     std::cout << ops_proxy[op].get_name() << ", ";
        // }
        // std::cout << std::endl;
        // //
        // // PRINTOUT***PRINTOUT***PRINTOUT

        if (path.size() > 0) { // To append, remove last state -> duplicate
            path.pop_back();
        }
        for (State s : best_sub_path) {
            path.emplace_back(s);
        }

        // std::cout << "Stitched plan" << std::endl;

        // Update step_state according to run cost + heuristic
        step_state = chosen_state;

        // std::cout << "Chosen state: " << chosen_state.get_id() << std::endl;

        if (task_properties::is_goal_state(task_proxy, step_state)) {
            // std::cout << "Positive goal check" << std::endl;
            assert(solution_found != -1);
            break;
        }
        // std::cout << "After goal check" << std::endl;
    }
    // Policy ordering is not yet implemented; currently returns an empty vector.
    return std::vector<int>(0);
}

/**
 * Start searching with a given plan. If plan is empty, start from the initial state using mult_search.
 * If it finds a goal, refinement is attempted by finding phases of against-heuristic choices.
 * Other policies are then tried from there.
 *
 * If no goal is found initially, other policies are started from against-heuristic choices.
 * THOUGHT: Could be composed with mult-policy search from the initial state.
 *
 * @returns Ranking of policies (TODO)
 */
std::vector<int> MultPolicySearch::imp_fault_search(std::vector<OperatorID> &plan, std::vector<State> &path) {
    (void)plan;
    (void)path;

    // Should REFINE or COMPLETE existing attempt
    if (path.empty() || !task_properties::is_goal_state(task_proxy, path.back())) {
        log_mult_pol() << "Test log message" << std::endl;
        // Initiate search from end of path;
        cut_at_duplicate_state(plan, path);
        State start_state = path.empty() ? initial_state : path.back();

        std::vector<OperatorID> ctd_plan;
        std::vector<State> ctd_path;
        imp_mult_search(start_state, ctd_plan, ctd_path);

        append_sub_plan(-1, plan, path, ctd_plan, ctd_path);
    }

    return std::vector<int>(0);
}

/**
 * Runs a variable number of policies, ordered by pol_ranking along a given path.
 * Can be seen as path refinement, however also applicable for path fragments (e.g. if no solution is found).
 */
void MultPolicySearch::imp_along_path(std::vector<OperatorID> &plan, std::vector<State> &path, std::vector<int> pol_ranking) {
    std::vector<int> accumulated_path_cost = accumulate_plan(plan);
    std::reverse(accumulated_path_cost.begin(), accumulated_path_cost.end());

    std::vector<OperatorID> sub_plan;
    std::vector<State> sub_path;

    for (int pol_num = 0; pol_num < 3; ++pol_num) {     //TODO: Remove arbitrary number 3
        for (int i = 0; i < path.size(); ++i) {
            State start = path[i];
            PolicyCost cost = remote_policy->compute_policy_cost(start, pol_ranking[pol_num]);
#ifndef NDEBUG
            std::cout << "Plan length: " << plan.size() << ", Path length: " << path.size() << std::endl;
            std::cout << "Acc path cost: " << accumulated_path_cost[i] << ", Policy Cost: " << remote_policy->compute_policy_cost(start, pol_ranking[pol_num]) << std::endl;
            PolicyCost pol_cost = remote_policy->compute_policy_cost(start, pol_ranking[pol_num]);
            assert(pol_cost == -1 || accumulated_path_cost[i] == pol_cost);
#endif
            if (cost != -1 && cost < accumulated_path_cost[i]) {
                std::cout << "Better subplan found." << std::endl;

                remote_policy->execute_get_transitions_and_path(start, sub_plan, sub_path, pol_ranking[pol_num]);
                append_sub_plan(i, plan, path, sub_plan, sub_path);
                accumulated_path_cost = accumulate_plan(plan);
                std::reverse(accumulated_path_cost.begin(), accumulated_path_cost.end());

#ifndef NDEBUG
                std::cout << "Verifying plan." << std::endl;
                bool is_verified = verify_plan(SearchAlgorithm::task, initial_state, plan);
                std::cout << "Is plan verified: " << is_verified << std::endl;
                assert(is_verified);
                // assert(plan.size() + 1 == path.size());
#endif
            }
        }
    }
}

// Only use for final (refined) plan execution? Outsource real logic into separate method?
SearchStatus MultPolicySearch::step() {
    // std::cout << "In step" << std::endl;
    if (check_goal_and_set_plan(search_state)) {
        // std::cout << "In goal" << std::endl;
        return SOLVED;
    }
    if (search_path.size() > path_index) {
        // std::cout << "Taking a step" << std::endl;
        search_state = search_path[path_index];
        SearchNode node = search_space.get_node(search_state);
        if (parent_id == StateID::no_state) {
            node.open_initial();
        } else {
            SearchNode parent_node = search_space.get_node(parent_state);
            OperatorProxy last_op = task_proxy.get_operators()[search_plan[path_index - 1]];
            node.open(parent_node, last_op, get_adjusted_cost(last_op));
        }
        node.close();

        parent_state = search_state;
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

// ************************ Miscellaneous **************************

std::vector<int> MultPolicySearch::accumulate_plan(std::vector<OperatorID> &plan) {
    std::vector<int> acc_costs;
    int akku = 0;
    acc_costs.push_back(akku);
    for (OperatorID id : plan) {
        akku += task_proxy.get_operators()[id].get_cost();
        acc_costs.push_back(akku);
    }

    return acc_costs;
}

/**
 * Appends sub_plan/sub_path starting at index (INCLUSIVE)
 * Index -1: Append without cutting
 */
void MultPolicySearch::append_sub_plan(int index, std::vector<OperatorID> &plan, std::vector<State> &path, std::vector<OperatorID> &sub_plan, std::vector<State> &sub_path) {
    if (index != -1) {
        plan.resize(index, OperatorID(-1));
        path.resize(index);
    }

    assert(path.back() == sub_path.back());

    for (OperatorID op : sub_plan) {
        plan.push_back(op);
    }
    for (State s : sub_path) {
        path.push_back(s);
    }
}

void MultPolicySearch::print_statistics() const {
    statistics.print_detailed_statistics();
    search_space.print_statistics();
}

void add_options_to_feature(plugins::Feature &feature) {
    SearchAlgorithm::add_pruning_option(feature);
    SearchAlgorithm::add_options_to_feature(feature);
}

/**
 * Cuts a path and corresponding plan at the first duplicate occurrence of a state.
 * If handling a non-complete fragment, i.e. plan and path are of same size, plan is cut by one.
 * Quadratic in runtime!!
 */
void MultPolicySearch::cut_at_duplicate_state(std::vector<OperatorID> &plan, std::vector<State> &path) {
    assert(path.size() >= plan.size());

    if (plan.size() == path.size() && !plan.empty()) {
        plan.pop_back();
    }
    // Check each state against all previous states
    for (size_t i = 0; i < path.size(); ++i) {
        for (size_t j = 0; j < i; ++j) {
            if (path[i] == path[j]) {
                // Found duplicate; cut at first occurrence
                log_mult_pol() << "Loop pruned at duplicate state" << std::endl;

                path.resize(j + 1);
                plan.resize(j, OperatorID(-1));
                return;
            }
        }
    }
}

std::ostream &MultPolicySearch::log_mult_pol() {
    std::cout << "[MPLog, t=" << utils::g_timer << "] ";
    return std::cout;
}
}
