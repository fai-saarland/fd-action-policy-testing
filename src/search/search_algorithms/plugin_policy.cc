#include "policy_search.h"
#include "search_common.h"

#include "../plugins/plugin.h"

using namespace std;

namespace plugin_policy {
class PolicySearchFeature : public plugins::TypedFeature<SearchAlgorithm, policy_search::PolicySearch> {
public:
    PolicySearchFeature() : TypedFeature("policy") {
        document_title("Policy search");
        document_synopsis("");

        // add_option<shared_ptr<OpenListFactory>>("open", "open list");
        // add_option<bool>(
        //     "reopen_closed",
        //     "reopen closed nodes",
        //     "false");
        // add_option<shared_ptr<Evaluator>>(
        //     "f_eval",
        //     "set evaluator for jump statistics. "
        //     "(Optional; if no evaluator is used, jump statistics will not be displayed.)",
        //     plugins::ArgumentInfo::NO_DEFAULT);
        // add_list_option<shared_ptr<Evaluator>>(
        //     "preferred",
        //     "use preferred operators of these evaluators",
        //     "[]");
        policy_search::add_options_to_feature(*this);
    }

    virtual shared_ptr<policy_search::PolicySearch> create_component(const plugins::Options &options, const utils::Context &) const override {
        shared_ptr<policy_search::PolicySearch> search_algorithm = make_shared<policy_search::PolicySearch>(options);

        return search_algorithm;
    }
};

static plugins::FeaturePlugin<PolicySearchFeature> _plugin;
}
