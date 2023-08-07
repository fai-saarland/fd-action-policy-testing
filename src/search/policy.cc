#include "policy.h"

#include "evaluation_context.h"
#include "option_parser.h"
#include "plugin.h"
#include "policy_result.h"

#include "task_utils/task_properties.h"
#include "tasks/cost_adapted_task.h"


Policy::Policy(const Options &opts)
    : policy_cache(PEntry()),
      cache_policy_values(opts.get<bool>("cache_estimates")),
      task(opts.get<std::shared_ptr<AbstractTask>>("transform")),
      task_proxy(*task) {
}

Policy::~Policy() {
}

PolicyResult Policy::compute_result(EvaluationContext &eval_context) {
    const State &state = eval_context.get_state();

    if (cache_policy_values && !policy_cache[state].dirty) {
        PolicyResult result;
        result.set_preferred_operators(std::vector<OperatorID>(policy_cache[state].operator_ids));
        result.set_operator_preferences(std::vector<float>(policy_cache[state].operator_preferences));
        result.set_count_evaluation(false);
        return result;
    } else {
        PolicyResult result = compute_policy(state);

        if (!result.get_preferred_operators().empty() &&
            result.get_operator_preferences().empty()) {
            result.set_operator_preferences(std::vector<float>(
                                                result.get_preferred_operators().size(),
                                                1.0 / result.get_preferred_operators().size()));
        }
        if (cache_policy_values) {
            policy_cache[state] = PEntry(result.get_preferred_operators(),
                                         result.get_operator_preferences());
        }
        result.set_count_evaluation(true);
        return result;
    }
}

State Policy::convert_ancestor_state(const State &ancestor_state) const {
    return task_proxy.convert_ancestor_state(ancestor_state);
}

void Policy::add_options_to_parser(OptionParser &parser) {
    parser.add_option<std::shared_ptr<AbstractTask>>(
        "transform",
        "Optional task transformation for the policy."
        " Currently, adapt_costs(), sampling_transform(), and no_transform() are "
        "available.",
        "no_transform()");
    parser.add_option<bool>("cache_estimates", "cache policy estimates", "true");
}

static PluginTypePlugin<Policy> _type_plugin(
    "Policy",
    // TODO: Add information for wiki page
    "");
