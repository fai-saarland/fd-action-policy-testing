#pragma once

#include <string>
#include <map>
#include <pheromone/policy_client.h>
#include "../../utils/exceptions.h"
#include "../../task_proxy.h"
#include "../../pruning_method.h"
#include "../policy.h"

namespace policy_testing {
class RemotePolicyError : public utils::Exception {
public:
    explicit RemotePolicyError(const std::string &msg);

    void print() const override;
};

class RemotePolicy : public Policy {
    inline static phrm_policy_t *g_pheromone_policy = nullptr;
    inline static std::shared_ptr<RemotePolicy> g_default_policy = nullptr;
    inline static int g_num_policy_models = 0;

public:
    RemotePolicy() = default;
    explicit RemotePolicy(const plugins::Options &opts);
    ~RemotePolicy() override;
    static void add_options_to_feature(plugins::Feature &feature);

    /**
     * Establishes a connection to the remote server.
     */
    static void establish_connection(const std::string &url);

    /**
     * Returns true iff a connection to the remote server is established.
     */
    static bool connection_established() {return g_pheromone_policy;}

    static std::shared_ptr<RemotePolicy> get_global_default_policy();

    /**
     * Returns FDR planning task in the Fast Downward format
     * https://www.fast-downward.org/TranslatorOutputFormat
     */
    static std::string input_fdr();

    /**
     * Apply policy on the state and retrieve the selected operator.
     */
    OperatorID apply(const State &state) override;
    OperatorID apply(const State &state, int model_index);
    static OperatorID static_apply(const State &state);
    static OperatorID static_apply(const State &state, int model_index);
    OperatorID majority_apply(const State &state);

    Policy::RunResult
    execute_get_plan_and_path(const State &state0, std::vector<OperatorID> &plan, std::vector<State> &path,
                              int model_index = 0, PolicyCost max_cost = -1, int max_steps = -1);

    /**
     * Runs the policy on state0 and fills plan and path vectors.
     *
     * @warning Does NOT delete plan, if no actual plan is found!
     */
    Policy::RunResult
    execute_get_transitions_and_path(const State &state0, std::vector<OperatorID> &plan, std::vector<State> &path,
                                     int model_index, PolicyCost max_cost = -1, int max_steps = -1);


    /**
     * Computes the policy cost; Policy::UNSOLVED if no goal is found and Policy::UNKNOWN if run is aborted.
     *
     * @param state State to start policy run on.
     * @param model_index Model index of the RemotePolicy to use. When set to -1, majority vote is applied.
     */
    PolicyCost
    compute_policy_cost(const State &state, int model_index = 0, PolicyCost max_cost = -1, int max_steps = -1);

    PolicyCost compute_policy_cost(const State &state);

    /**
     * Returns the number of loaded policy model served by the remote policy.
     */
    static int get_num_models() {
        return g_num_policy_models;
    }

private:
    inline static std::unique_ptr<PerStateInformation<std::vector<PolicyCost>>> policy_cost_cache{};
    inline static std::unique_ptr<PerStateInformation<std::vector<OperatorID>>> policy_operator_cache{};
};

/**
* Class implementing pruning based on remote policy.
* Only works with global policy created from the main program using the --remote-policy option.
*/
class RemotePolicyPruning : public PruningMethod {
public:
    explicit RemotePolicyPruning(const plugins::Options &opts);
    void prune_operators(const State &state, std::vector<OperatorID> &op_ids) override;
    void prune(const State &, std::vector<OperatorID> &) override {
        std::cerr << "RemotePolicyPruning::prune is not implemented, use prune_operators instead";
        utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
    }
    void print_statistics() const override;
};
} /* namespace policy_testing*/
