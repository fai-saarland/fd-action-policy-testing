#include "remote_policy.h"

#include "../option_parser.h"
#include "../plugin.h"
#include <utility>

namespace policy_testing {
RemotePolicyError::RemotePolicyError(std::string msg) : msg(std::move(msg)) {}

void RemotePolicyError::print() const {
    std::cerr << "Remote Policy Error: " << msg << std::endl;
}

RemotePolicy::RemotePolicy(const options::Options &opts) : Policy(opts) {}

RemotePolicy::~RemotePolicy() {
    if (pheromone_policy) {
        phrmPolicyDel(pheromone_policy);
    }
}

void RemotePolicy::add_options_to_parser(options::OptionParser &parser) {
    Policy::add_options_to_parser(parser);
    parser.add_option<std::string>("url", "URL to connect to", "");
}

void RemotePolicy::establish_connection(const std::string &url) {
    utils::g_log << "Establishing connection to remote policy at " << url << std::endl;
    pheromone_policy = phrmPolicyConnect(url.c_str());
    if (!pheromone_policy) {
        throw RemotePolicyError("Cannot connect to " + url);
    }
    utils::g_log << "Connection to " << url << " established" << std::endl;
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
    // TODO (Jan) added this line, check if unpack is really necessary
    state_in.unpack();
    const std::vector<int> &state = state_in.get_values();
    int op_id =
        phrmPolicyFDRStateOperator(pheromone_policy, state.data(), state.size());
    if (op_id < 0) {
        return OperatorID::no_operator;
    }
    return OperatorID(op_id);
}

OperatorID RemotePolicy::apply(const State &state_in) {
    return static_apply(state_in);
}

RemotePolicyPruning::RemotePolicyPruning(options::Options &) : PruningMethod() {}

void RemotePolicyPruning::prune_operators(const State &state,
                                          std::vector<OperatorID> &op_ids) {
    OperatorID policy_op_id = RemotePolicy::static_apply(state);
    op_ids.clear();
    if (policy_op_id != OperatorID::no_operator) {
        op_ids.push_back(policy_op_id);
    }
}

void RemotePolicyPruning::print_statistics() const {
    // TODO
}

static std::shared_ptr<PruningMethod> _parse_pruning(OptionParser &parser) {
    parser.document_synopsis("Remote policy pruning", "");
    Options opts = parser.parse();
    if (parser.dry_run()) {
        return nullptr;
    }
    return std::make_shared<RemotePolicyPruning>(opts);
}

static Plugin<PruningMethod> _plugin_pruning("remote_policy_pruning",
                                             _parse_pruning);
static Plugin<Policy> _plugin("remote_policy",
                              options::parse<Policy, RemotePolicy>);
} /* namespace policy_testing */
