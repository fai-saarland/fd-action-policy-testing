# pragma once

#include <string>
#include <pheromone/policy_client.h>
#include "../utils/exceptions.h"
#include "../task_proxy.h"
#include "../pruning_method.h"

namespace policy_testing {
class RemotePolicyError : public utils::Exception {
    std::string msg;
public:
    explicit RemotePolicyError(std::string msg);

    void print() const override;
};

class RemotePolicy {
    phrm_policy_t *_policy;

public:
    explicit RemotePolicy(const std::string &url);
    ~RemotePolicy();

    /**
     * Returns FDR planning task in the Fast Downward format
     * https://www.fast-downward.org/TranslatorOutputFormat
     */
    std::string input_fdr();

    /**
     * Apply policy on the state and retrieve the selected operator.
     */
    OperatorID apply_on_state(const State &state);
};

/**
 * Global policy created from the main program using the --remote-policy
 * option.
 */
extern std::shared_ptr<RemotePolicy> g_policy;

class RemotePolicyPruning : public PruningMethod {
public:
    explicit RemotePolicyPruning(options::Options &opts);
    void prune_operators(const State &state, std::vector<OperatorID> &op_ids) override;
    void print_statistics() const override;
};
} /* namespace policy_testing*/
