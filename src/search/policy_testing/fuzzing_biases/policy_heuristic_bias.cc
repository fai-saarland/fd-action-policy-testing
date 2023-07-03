#include "policy_heuristic_bias.h"
#include "../plugin.h"
#include "../../task_utils/task_properties.h"
#include "../../evaluation_context.h"
#include "../../heuristics/relaxation_heuristic.h"

namespace policy_testing {
policyHeuristicBias::policyHeuristicBias(const options::Options &opts)
    : PolicyBasedBias(opts),
      h(opts.get<std::shared_ptr<Evaluator>>("h")) {
}

void
policyHeuristicBias::add_options_to_parser(options::OptionParser &parser) {
    parser.add_option<std::shared_ptr<Evaluator>>("h");
    PolicyBasedBias::add_options_to_parser(parser);
}

int
policyHeuristicBias::bias(const State &state, unsigned int budget) {
    std::vector<State> path = policy->execute_get_path_fragment(state, get_step_limit(budget), false);
    int max_value = 0;

    // vector of size path.size() - 1 with the cost of the action chosen by the policy in each state
    // needs to be summed up in order to obtain path costs
    const std::vector<int> action_costs = policy->read_path_action_costs(path);
    std::vector<int> path_costs(action_costs.size());
    int current_path_cost = 0;
    for (int i = action_costs.size() - 1; i >= 0; --i) {
        current_path_cost += action_costs[i];
        path_costs[i] = current_path_cost;
    }

    for (int i = 0; i < path.size() - 1; ++i) {
        EvaluationContext context(path[i]);
        EvaluationResult result = h->compute_result(context);
        int h_value = result.get_evaluator_value();
        if (h_value == Heuristic::DEAD_END) {
            return (i == 0) ? NEGATIVE_INFINITY : POSITIVE_INFINITY;
        } else if (h_value == Heuristic::NO_VALUE) {
            continue;
        }
        max_value = std::max<int>(max_value, path_costs[i] - h_value);
    }
    return max_value;
}

bool
policyHeuristicBias::can_exclude_state(const State &s) {
    EvaluationContext context(s);
    EvaluationResult result = h->compute_result(context);
    return result.is_infinite();
}

static Plugin<FuzzingBias> _plugin_bias_policyHeuristicBias("policy_heuristic_bias", options::parse<PolicyBasedBias,
                                                                                                    policyHeuristicBias>);
} // namespace policy_testing
