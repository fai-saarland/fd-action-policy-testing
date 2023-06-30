#include "remote_policy.h"
#include "../option_parser.h"
#include "../plugin.h"

namespace policy_fuzzing {

std::shared_ptr<RemotePolicy> g_policy = nullptr;

RemotePolicyError::RemotePolicyError(const std::string &msg)
    : msg(msg)
{
}

void RemotePolicyError::print() const
{
    std::cerr << "Remote Policy Error: " << msg << std::endl;
}

RemotePolicy::RemotePolicy(const std::string &url)
{
    _policy = phrmPolicyConnect(url.c_str());
    if (_policy == NULL){
        throw RemotePolicyError("Cannot connetct to " + url);
    }
}

RemotePolicy::~RemotePolicy()
{
    if (_policy != NULL)
        phrmPolicyDel(_policy);
}

std::string RemotePolicy::input_fdr()
{
    char *fdr = phrmPolicyFDRTaskFD(_policy);
    if (fdr == NULL){
        throw RemotePolicyError("Cannot obtain FDR task");
    }

    std::string out(fdr);
    free(fdr);
    return out;
}

OperatorID RemotePolicy::apply_on_state(const State &state_in)
{
    const std::vector<int> &state = state_in.get_values();
    int op_id = phrmPolicyFDRStateOperator(_policy, state.data(), state.size());
    if (op_id < 0)
        return OperatorID::no_operator;
    return OperatorID(op_id);
}



RemotePolicyPruning::RemotePolicyPruning(options::Options &opts)
        : PruningMethod()
{
    (void) opts;
}

void RemotePolicyPruning::prune_operators(
        const State &state, std::vector<OperatorID> &op_ids) {
    OperatorID policy_op_id = g_policy->apply_on_state(state);
    op_ids.clear();
    if (policy_op_id != OperatorID::no_operator)
        op_ids.push_back(policy_op_id);
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

static Plugin<PruningMethod> _plugin_pruning("remote_policy_pruning", _parse_pruning);

} /* namespace policy_fuzzing */
