# pragma once

#include <string>
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
    inline static phrm_policy_t *pheromone_policy = nullptr;
    inline static std::shared_ptr<RemotePolicy> g_default_policy = nullptr;

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
     * Establishes a connection to the remote server.
     */
    static bool connection_established() {return pheromone_policy;}

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
    static OperatorID static_apply(const State &state);
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
