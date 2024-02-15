#include "surface_bias.h"
#include "../../plugins/plugin.h"
#include "../../evaluation_context.h"

namespace policy_testing {
SurfaceBias::SurfaceBias(const plugins::Options &opts)
    : PolicyBasedBias(opts),
      h(opts.contains("h") ? opts.get<std::shared_ptr<Evaluator>>("h") : nullptr),
      internalPlanCostEstimator(opts.contains("ipo") ?std::dynamic_pointer_cast<InternalPlannerPlanCostEstimator>(
                                    opts.get<std::shared_ptr<PlanCostEstimator>>("ipo")) : nullptr),
      omit_maximization(opts.get<bool>("omit_maximization")) {
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
SurfaceBias::add_options_to_feature(plugins::Feature &feature) {
    feature.add_option<std::shared_ptr<Evaluator>>("h", "", plugins::ArgumentInfo::NO_DEFAULT);
    feature.add_option<std::shared_ptr<PlanCostEstimator>>("ipo", "", plugins::ArgumentInfo::NO_DEFAULT);
    feature.add_option<bool>("omit_maximization",
                             "do not maximize over all subpaths, only consider first and last state", "false");
    PolicyBasedBias::add_options_to_feature(feature);
}

int
SurfaceBias::bias_without_maximization(const State &state, unsigned int budget) {
    const std::vector<State> complete_path = policy->execute_get_path_fragment(state, get_step_limit(budget), false);
    if (complete_path.empty()) {
        return NEGATIVE_INFINITY;
    }

    // work with a subpath only consisting of first and last state (if possible)
    std::vector<State> restricted_path;
    restricted_path.push_back(complete_path.front());
    if (complete_path.size() > 1) {
        restricted_path.push_back(complete_path.back());
    }

    // compute h_values for first and last state and handle special case of infinite h_values
    std::vector<int> h_values;
    bool first = true;
    for (const State &s : restricted_path) {
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

    if (restricted_path.size() < 2) {
        return NEGATIVE_INFINITY;
    }
    assert(h_values.size() == 2);
    const int path_cost = policy->read_accumulated_path_action_cost(restricted_path);
    return path_cost - (h_values[0] - h_values[1]);
}

int
SurfaceBias::bias_with_maximization(const State &state, unsigned int budget) {
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

    if (path.size() < 2) {
        return NEGATIVE_INFINITY;
    }

    int max_value = NEGATIVE_INFINITY;
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

int
SurfaceBias::bias(const State &state, unsigned int budget) {
    if (omit_maximization) {
        return bias_without_maximization(state, budget);
    } else {
        return bias_with_maximization(state, budget);
    }
}

bool SurfaceBias::can_exclude_state(const State &s) {
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

class SurfaceBiasFeature : public plugins::TypedFeature<FuzzingBias, SurfaceBias> {
public:
    SurfaceBiasFeature() : TypedFeature("surface_bias") {
        SurfaceBias::add_options_to_feature(*this);
    }
};
static plugins::FeaturePlugin<SurfaceBiasFeature> _plugin;
} // namespace policy_testing
