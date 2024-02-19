#include "detour_bias.h"
#include "../../plugins/plugin.h"
#include "../../evaluation_context.h"


namespace policy_testing {
DetourBias::DetourBias(const plugins::Options &opts) :
    PolicyBasedBias(opts),
    h(opts.contains("h") ?
      std::dynamic_pointer_cast<relaxation_heuristic::RelaxationHeuristic>(
          opts.get<std::shared_ptr<Evaluator>>("h")) : nullptr),
    internalPlanCostEstimator(opts.contains("ipo") ? std::dynamic_pointer_cast<InternalPlannerPlanCostEstimator>(
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
DetourBias::add_options_to_feature(plugins::Feature &feature) {
    feature.add_option<std::shared_ptr<Evaluator>>("h", "", plugins::ArgumentInfo::NO_DEFAULT);
    feature.add_option<std::shared_ptr<PlanCostEstimator>>("ipo", "", plugins::ArgumentInfo::NO_DEFAULT);
    feature.add_option<bool>("omit_maximization",
                             "do not maximize over all sub-paths, only consider first and last state", "false");
    PolicyBasedBias::add_options_to_feature(feature);
}

int
DetourBias::bias_without_maximization(const State &state, unsigned int budget) {
    std::vector<State> path = policy->execute_get_path_fragment(state, get_step_limit(budget), false);
    if (path.size() < 2) {
        return NEGATIVE_INFINITY;
    }
    const int path_cost = policy->read_accumulated_path_action_cost(path);
    if (h) {
        const int h_value = h->compute_path_heuristic(path.front(), path.back());
        if (h_value == Heuristic::DEAD_END || h_value == Heuristic::NO_VALUE) {
            // this case should never occur for a safe heuristic
            return NEGATIVE_INFINITY;
        }
        return path_cost - h_value;
    } else {
        assert(internalPlanCostEstimator);
        const int h_value = internalPlanCostEstimator->compute_trusted_value_with_cache(path.front(), &path.back());
        assert(h_value != PlanCostEstimator::ReturnCode::DEAD_END);
        return path_cost - h_value;
    }
}

int
DetourBias::bias_with_maximization(const State &state, unsigned int budget) {
    std::vector<State> path = policy->execute_get_path_fragment(state, get_step_limit(budget), false);
    if (path.size() < 2) {
        return NEGATIVE_INFINITY;
    }
    const std::vector<int> action_costs = policy->read_path_action_costs(path);
    int max_value = NEGATIVE_INFINITY;
    for (int i = 0; i < path.size() - 1; ++i) {
        const State &s_i = path[i];
        int path_fragment_cost = 0; // the cost between s_i and s_j
        for (int j = i + 1; j < path.size(); ++j) {
            path_fragment_cost += action_costs[j - 1];
            const State &s_j = path[j];
            int h_value;
            if (h) {
                h_value = h->compute_path_heuristic(s_i, s_j);
                if (h_value == Heuristic::DEAD_END || h_value == Heuristic::NO_VALUE) {
                    // this case should never occur for a safe heuristic
                    continue;
                }
            } else {
                assert(internalPlanCostEstimator);
                h_value = internalPlanCostEstimator->compute_trusted_value_with_cache(s_i, &s_j);
                assert(h_value != PlanCostEstimator::ReturnCode::DEAD_END);
            }
            max_value = std::max<int>(max_value, path_fragment_cost - h_value);
        }
    }
    return max_value;
}

int
DetourBias::bias(const State &state, unsigned int budget) {
    if (omit_maximization) {
        return bias_without_maximization(state, budget);
    } else {
        return bias_with_maximization(state, budget);
    }
}

bool DetourBias::can_exclude_state(const State &) {
    return false;
}

class DetourBiasFeature : public plugins::TypedFeature<FuzzingBias, DetourBias> {
public:
    DetourBiasFeature() : TypedFeature("detour_bias") {
        DetourBias::add_options_to_feature(*this);
    }
};
static plugins::FeaturePlugin<DetourBiasFeature> _plugin;
} // namespace policy_testing
