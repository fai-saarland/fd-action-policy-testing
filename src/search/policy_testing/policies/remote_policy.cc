#include "remote_policy.h"

#include "../../plugins/plugin.h"
#include <utility>

namespace policy_testing {
RemotePolicyError::RemotePolicyError(const std::string &msg) : utils::Exception(msg) {}

void RemotePolicyError::print() const {
    std::cerr << "Remote Policy Error: " << msg << std::endl;
}

RemotePolicy::RemotePolicy(const plugins::Options &opts) : Policy(opts) {}

RemotePolicy::~RemotePolicy() {
    if (pheromone_policy) {
        phrmPolicyDel(pheromone_policy);
    }
}

void RemotePolicy::add_options_to_feature(plugins::Feature &feature) {
    Policy::add_options_to_feature(feature);
}

void RemotePolicy::establish_connection(const std::string &url) {
    utils::g_log << "Establishing connection to remote policy at " << url << std::endl;
    pheromone_policy = phrmPolicyConnect(url.c_str());
    if (!pheromone_policy) {
        throw RemotePolicyError("Cannot connect to " + url);
    }
    utils::g_log << "Connection to " << url << " established" << std::endl;
    g_default_policy = std::make_shared<RemotePolicy>();
}

std::shared_ptr<RemotePolicy> RemotePolicy::get_global_default_policy() {
    if (!pheromone_policy) {
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
    char *fdr = phrmPolicyFDRTaskFD(pheromone_policy);
    if (!fdr) {
        throw RemotePolicyError("Cannot obtain FDR task");
    }
    std::string out(fdr);
    free(fdr);
    return out;
}

OperatorID RemotePolicy::static_apply(const State &state_in) {
    if (!connection_established()) {
        throw RemotePolicyError("No connection to remote policy established.\n"
                                "Make sure your FD call starts with --remote-policy <url>.");
    }
    const std::vector<int> &state = state_in.get_values();
    int op_id =
        phrmPolicyFDRStateOperator(pheromone_policy, state.data(), state.size());
    if (op_id >= 0) {
        return OperatorID(op_id);
    } else if (op_id == -1) {
        return OperatorID::no_operator;
    } else {
        std::cerr << "phrmPolicyFDRStateOperator failed" << std::endl;
        utils::exit_with(utils::ExitCode::REMOTE_POLICY_ERROR);
    }
}

OperatorID RemotePolicy::apply(const State &state_in) {
    return static_apply(state_in);
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
