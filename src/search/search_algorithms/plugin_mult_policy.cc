#include "mult_policy_search.h"
#include "search_common.h"

#include "../plugins/plugin.h"
#include "../evaluator.h"

using namespace std;

namespace plugin_policy {
class MultPolicySearchFeature : public plugins::TypedFeature<SearchAlgorithm, mult_policy_search::MultPolicySearch> {
public:
    MultPolicySearchFeature() : TypedFeature("mult_policy") {
        document_title("Policy search with multiple policies");
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
        add_option<bool>(
            "along_path",
            "Run policies along first policy's path.",
            "false"
            );
        add_option<bool>(
            "tree_search",
            "Initially conduct multi-policy tree search with heuristic.",
            "false"
            );
        add_option<bool>(
            "fault_detection",
            "Use heuristic to detect suboptimal behavior and switch policy.",
            "false"
            );
        add_option<std::shared_ptr<Evaluator>>(
            "h",
            "Heuristic for tree search.",
            "lmcut()"
            );
        add_option<int>(
            "step_depth",
            "How deep each tree search step should be.",
            "10"
            );
        add_option<int>(
            "num_policies",
            "Number of policies to run (for all search types).",
            "5"
            );
        add_option<bool>(
            "prefer_solved",
            "Whether the search should prefer finding any plan at all (qualitative), instead of optimizing for cost.",
            "false"
            );
        mult_policy_search::add_options_to_feature(*this);
    }

    virtual shared_ptr<mult_policy_search::MultPolicySearch> create_component(const plugins::Options &options, const utils::Context &) const override {
        shared_ptr<mult_policy_search::MultPolicySearch> search_algorithm = make_shared<mult_policy_search::MultPolicySearch>(options);

        return search_algorithm;
    }
};

static plugins::FeaturePlugin<MultPolicySearchFeature> _plugin;
}
