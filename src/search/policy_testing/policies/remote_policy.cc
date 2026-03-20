#include "remote_policy.h"

#include "../../plugins/plugin.h"
#include <utility>
#include "../../task_utils/task_properties.h"
#include "../utils/custom_exceptions.h"

namespace policy_testing {
// Define the static member outside the class
// PerStateInformation<std::vector<PolicyCost>> RemotePolicy::policy_cost_cache(std::vector<PolicyCost>(30, Policy::UNKNOWN));
// PerStateInformation<std::vector<OperatorID>> RemotePolicy::policy_operator_cache(std::vector<OperatorID>(30, OperatorID(-1)));

RemotePolicyError::RemotePolicyError(const std::string &msg) : utils::Exception(msg) {}

void RemotePolicyError::print() const {
    std::cerr << "Remote Policy Error: " << msg << std::endl;
}

RemotePolicy::RemotePolicy(const plugins::Options &opts)
    : Policy(opts)
{}

RemotePolicy::~RemotePolicy() {
    if (g_pheromone_policy) {
        phrmPolicyDel(g_pheromone_policy);
    }
}

void RemotePolicy::add_options_to_feature(plugins::Feature &feature) {
    Policy::add_options_to_feature(feature);
}

void RemotePolicy::establish_connection(const std::string &url) {
    utils::g_log << "Establishing connection to remote policy at " << url << std::endl;
    g_pheromone_policy = phrmPolicyConnect(url.c_str());
    if (!g_pheromone_policy) {
        throw RemotePolicyError("Cannot connect to " + url);
    }
    utils::g_log << "Connection to " << url << " established" << std::endl;
    g_default_policy = std::make_shared<RemotePolicy>();
    g_num_policy_models = phrmPolicyNumModels(g_pheromone_policy);

    policy_cost_cache = std::make_unique<PerStateInformation<std::vector<PolicyCost>>>(std::vector<PolicyCost>(get_num_models(), Policy::UNKNOWN));
    policy_operator_cache = std::make_unique<PerStateInformation<std::vector<OperatorID>>>(std::vector<OperatorID>(get_num_models(), OperatorID(-1)));

    utils::g_log << "Policy server provides " << g_num_policy_models << " model(s)." << std::endl;
}

std::shared_ptr<RemotePolicy> RemotePolicy::get_global_default_policy() {
    if (!g_pheromone_policy) {
        throw RemotePolicyError("Global default policy not available, no connection established");
    }
    assert(g_default_policy);
    return g_default_policy;
}

std::string RemotePolicy::input_fdr() {
    if (!connection_established()) {
        throw RemotePolicyError("No connection to remote policy established.\n"
                                "Make sure your FD call starts with --remote-policy <url>.");
    }
    char *fdr = phrmPolicyFDRTaskFD(g_pheromone_policy);
    if (!fdr) {
        throw RemotePolicyError("Cannot obtain FDR task");
    }
    std::string out(fdr);
    free(fdr);
    return out;
}

OperatorID RemotePolicy::static_apply(const State &state_in, int model_index) {
    if (!connection_established()) {
        throw RemotePolicyError("No connection to remote policy established.\n"
                                "Make sure your FD call starts with --remote-policy <url>.");
    }
    const std::vector<int> &state = state_in.get_values();
    int op_id = phrmPolicyFDRStateOperator(g_pheromone_policy, state.data(), state.size(), model_index);
    if (op_id >= 0) {
        return OperatorID(op_id);
    } else if (op_id == -1) {
        return OperatorID::no_operator;
    } else {
        std::cerr << "phrmPolicyFDRStateOperator failed" << std::endl;
        utils::exit_with(utils::ExitCode::REMOTE_POLICY_ERROR);
    }
}

OperatorID RemotePolicy::static_apply(const State &state_in) {
    return static_apply(state_in, 0);
}

OperatorID RemotePolicy::apply(const State &state_in) {
    return apply(state_in, 0);
}

OperatorID RemotePolicy::apply(const State &state_in, int model_index) {
    if ((*policy_operator_cache)[state_in][model_index] != OperatorID(-1)) {
        return OperatorID{(*policy_operator_cache)[state_in][model_index]};
    }
    OperatorID result = static_apply(state_in, model_index);
    (*policy_operator_cache)[state_in][model_index] = result;
    return result;
}

OperatorID RemotePolicy::majority_apply(const State &state_in) {
    std::vector<int> operator_ids;
    for (int model = 0; model < get_num_models(); ++model) {
        operator_ids.push_back(apply(state_in, model).get_index());
    }

    assert(operator_ids.size() != 0);
    std::sort(operator_ids.begin(), operator_ids.end());

    int max_count = 1, res = operator_ids[0], curr_count = 1;
    for (int i = 1; i < operator_ids.size(); i++) {
        if (operator_ids[i] == operator_ids[i - 1])
            curr_count++;
        else
            curr_count = 1;

        if (curr_count > max_count) {
            max_count = curr_count;
            res = operator_ids[i - 1];
        }
    }
    return OperatorID(res);
}

Policy::RunResult
RemotePolicy::execute_get_plan_and_path(const State &state0, std::vector<OperatorID> &plan, std::vector<State> &path,
                                        int model_index, PolicyCost max_cost, int max_steps) {
    RunResult run_result = execute_get_transitions_and_path(state0, plan, path, model_index, max_cost, max_steps);
    if (!run_result.complete || !run_result.solves_state) {
        plan.clear();
    }
    return run_result;
}

Policy::RunResult
RemotePolicy::execute_get_transitions_and_path(const State &state0, std::vector<OperatorID> &plan, std::vector<State> &path,
                                               int model_index, PolicyCost max_cost, int max_steps) {
    assert(plan.empty());
    assert(path.empty());

    const bool cost_limit_set = max_cost >= 0;
    const bool step_limit_set = max_steps >= 0;

    utils::HashSet<StateID> seen;
    seen.insert(state0.get_id());
    State state = state0;
    PolicyCost current_cost = 0;

    for (unsigned int step = 0; (!cost_limit_set || current_cost < max_cost) && (!step_limit_set || step <= max_steps); ++step) {
        path.push_back(state);
        if (task_properties::is_goal_state(get_task_proxy(), state)) {
            return {true, true};
        }
        if (are_limits_reached()) {
            throw OutOfResourceException();
        }
        OperatorID op = model_index == -1 ? majority_apply(state) : apply(state, model_index);
        if (op == NO_OPERATOR) {
            return {true, false};
        }
        plan.push_back(op);
        state = get_successor_state(state, op);
        if (!seen.insert(state.get_id()).second) {
            // run loops
            return {true, false};
        }
        current_cost += get_operator_cost(op);
    }
    // Step or cost limit reached
    return {false, false};
}

PolicyCost
RemotePolicy::compute_policy_cost(const State &state, int model_index, PolicyCost max_cost, int max_steps) {
    // Handle majority vote
    if (model_index == -1) {
        std::vector<OperatorID> plan;
        std::vector<State> path;
        const auto [result_known, solved] = execute_get_plan_and_path(state, plan, path, model_index, max_cost, max_steps);
        if (!result_known) {
            return UNKNOWN;
        }
        PolicyCost plan_cost = solved ? calculate_plan_cost(get_task(), plan) : UNSOLVED;
        return plan_cost;
    }

    // No majority vote: execute policy and handle caching
    // TODO: pretty ugly; how to instantiate with default value?
    // std::vector<PolicyCost> &policy_costs = policy_cost_cache[state];
    // if (policy_costs.empty()) {
    //     // std::cout << "Vector empty" << std::endl;
    //     policy_costs = std::vector<PolicyCost>(get_num_models(), UNKNOWN);
    // }
    PolicyCost &cost_cache_start = (*policy_cost_cache)[state][model_index];
    // std::cout << "Value in cost cache: " << policy_costs[model_index] << std::endl;

    if (cost_cache_start == UNKNOWN) {
        std::vector<OperatorID> plan;
        std::vector<State> path;
        const auto [result_known, solved] = execute_get_plan_and_path(state, plan, path, model_index, max_cost, max_steps);
        if (!result_known) {
            return UNKNOWN;
        }
        PolicyCost remaining_cost = solved ? calculate_plan_cost(get_task(), plan) : UNSOLVED;
        cost_cache_start = remaining_cost;
        if (!plan.empty()) {
            for (unsigned int path_index = 1; path_index < path.size(); ++path_index) {
                // subtract cost from previous step
                if (remaining_cost != UNSOLVED) {
                    remaining_cost -= get_operator_cost(plan[path_index - 1]);
                }
                // update cost of intermediate state
                const auto &intermediate_state = path[path_index];

                // TODO: pretty ugly; how to do with default value?
                // std::vector<PolicyCost> &intermediate_costs = policy_cost_cache[intermediate_state];
                // if (intermediate_costs.empty()) {
                //     intermediate_costs = std::vector<PolicyCost>(get_num_models(), UNKNOWN);
                // }
                PolicyCost &cost_cache_intermediate = (*policy_cost_cache)[intermediate_state][model_index];

                if (cost_cache_intermediate == UNKNOWN) {
                    cost_cache_intermediate = remaining_cost;
                } else {
                    assert(remaining_cost == cost_cache_intermediate);
                    break;
                }
            }
        }
    }
    return cost_cache_start;
}

PolicyCost
RemotePolicy::compute_policy_cost(const State &state) {
    return compute_policy_cost(state, 0);
}

RemotePolicyPruning::RemotePolicyPruning(const plugins::Options &opts) : PruningMethod(opts) {}

void RemotePolicyPruning::prune_operators(const State &state,
                                          std::vector<OperatorID> &op_ids) {
    OperatorID policy_op_id = RemotePolicy::static_apply(state);
    op_ids.clear();
    if (policy_op_id != OperatorID::no_operator) {
        op_ids.push_back(policy_op_id);
    }
}

void RemotePolicyPruning::print_statistics() const {
}

class RemotePolicyPruningFeature : public plugins::TypedFeature<PruningMethod, RemotePolicyPruning> {
public:
    RemotePolicyPruningFeature() : TypedFeature("remote_policy_pruning") {
        add_pruning_options_to_feature(*this);
    }
};
static plugins::FeaturePlugin<RemotePolicyPruningFeature> _remote_policy_pruning_plugin;

class RemotePolicyFeature : public plugins::TypedFeature<Policy, RemotePolicy> {
public:
    RemotePolicyFeature() : TypedFeature("remote_policy") {
        RemotePolicy::add_options_to_feature(*this);
    }
};
static plugins::FeaturePlugin<RemotePolicyFeature> _remote_policy_plugin;
} /* namespace policy_testing */
