#include "surface_bias.h"
#include "../plugin.h"
#include "../../evaluation_context.h"

namespace policy_testing {
surfaceBias::surfaceBias(const options::Options &opts)
    : PolicyBasedBias(opts),
      h(opts.contains("h") ? opts.get<std::shared_ptr<Evaluator>>("h") : nullptr),
      internalPlanCostEstimator(opts.contains("ipo") ?
                                std::dynamic_pointer_cast<InternalPlannerPlanCostEstimator>(opts.get<std::shared_ptr<PlanCostEstimator>>(
                                                                                                "ipo")) : nullptr) {
    if (internalPlanCostEstimator) {
        register_sub_component(internalPlanCostEstimator.get());
    }
    if ((!h && !internalPlanCostEstimator) || (opts.contains("h") && opts.contains("ipo"))) {
        std::cerr << "Surface Bias needs either a heuristic or an internal planner oracle (and not both)\nh must be"
            " a RelaxationHeuristic and ipo must be an InternalPlannerPlanCostEstimator" << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
    }
    if (internalPlanCostEstimator && internalPlanCostEstimator->continue_after_time_out) {
        std::cerr << "Do not use continue_after_timeout in the configuration of the internalPlanCostEstimator. "
            "States would be classified as dead ends if the planner times out." << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
    }
}

void
surfaceBias::add_options_to_parser(options::OptionParser &parser) {
    parser.add_option<std::shared_ptr<Evaluator>>("h", "", options::OptionParser::NONE);
    parser.add_option<std::shared_ptr<PlanCostEstimator>>("ipo", "", options::OptionParser::NONE);
    PolicyBasedBias::add_options_to_parser(parser);
}

int
surfaceBias::bias(const State &state, unsigned int budget) {
    const std::vector<State> path = policy->execute_get_path_fragment(state, get_step_limit(budget), false);

    // compute all h_values and handle special case of infinite h_values
    std::vector<int> h_values;
    bool first = true;
    for (const State &s : path) {
        if (h) {
            EvaluationContext context(s);
            EvaluationResult result = h->compute_result(context);
            if (result.is_infinite()) {
                return first ? NEGATIVE_INFINITY : POSITIVE_INFINITY;
            }
            h_values.push_back(result.get_evaluator_value());
        } else {
            assert(internalPlanCostEstimator);
            const int result = internalPlanCostEstimator->compute_trusted_value_with_cache(s);
            if (result == PlanCostEstimator::ReturnCode::DEAD_END) {
                return first ? NEGATIVE_INFINITY : POSITIVE_INFINITY;
            }
            h_values.push_back(result);
        }
        first = false;
    }

    int max_value = 0;
    const std::vector<int> action_costs = policy->read_path_action_costs(path);
    for (int i = 0; i < path.size() - 1; ++i) {
        int path_fragment_cost = 0; // the cost between s_i and s_j
        for (int j = i + 1; j < path.size(); ++j) {
            path_fragment_cost += action_costs[j - 1];
            max_value = std::max<int>(max_value, path_fragment_cost - (h_values[i] - h_values[j]));
        }
    }
    return max_value;
}

bool surfaceBias::can_exclude_state(const State &s) {
    if (h) {
        EvaluationContext context(s);
        EvaluationResult result = h->compute_result(context);
        return result.is_infinite();
    } else {
        assert(internalPlanCostEstimator);
        return internalPlanCostEstimator->compute_trusted_value_with_cache(s) ==
               PlanCostEstimator::ReturnCode::DEAD_END;
    }
}

static Plugin<FuzzingBias> _plugin_bias_surfaceBias(
    "surface_bias",
    options::parse<PolicyBasedBias, surfaceBias>);
} // namespace policy_testing
