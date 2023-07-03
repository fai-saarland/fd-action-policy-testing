#include "loopiness_bias.h"
#include "../../option_parser.h"
#include "../plugin.h"
#include "../../task_utils/task_properties.h"
#include "../../evaluation_context.h"


namespace policy_testing {
loopinessBias::loopinessBias(const options::Options &opts) :
    PolicyBasedBias(opts),
    h(opts.contains("h") ?
      std::dynamic_pointer_cast<relaxation_heuristic::RelaxationHeuristic>(opts.get<std::shared_ptr<Evaluator>>(
                                                                               "h")) : nullptr),
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
loopinessBias::add_options_to_parser(options::OptionParser &parser) {
    parser.add_option<std::shared_ptr<Evaluator>>("h", "", options::OptionParser::NONE);
    parser.add_option<std::shared_ptr<PlanCostEstimator>>("ipo", "", options::OptionParser::NONE);
    PolicyBasedBias::add_options_to_parser(parser);
}

int
loopinessBias::bias(const State &state, unsigned int budget) {
    std::vector<State> path = policy->execute_get_path_fragment(state, get_step_limit(budget), false);
    // select a subset of the indices of path; guaranteed to be ordered
    const std::vector<int> selected_indices = testing_rng.select_random_positions(path, (size_t)sqrt(path.size()));
    const std::vector<int> action_costs = policy->read_path_action_costs(path);

    int max_value = 0;
    for (int i = 0; i < selected_indices.size() - 1; ++i) {
        const int path_start_index = selected_indices[i];
        const State &s_i = path[path_start_index];
        for (int j = i + 1; j < selected_indices.size(); ++j) {
            const int path_target_index = selected_indices[j];
            const State &s_j = path[path_target_index];
            int h_value;
            if (h) {
                h_value = h->compute_path_heuristic(s_i, s_j);
                if (h_value == Heuristic::DEAD_END || h_value == Heuristic::NO_VALUE) {
                    continue;
                }
            } else {
                assert(internalPlanCostEstimator);
                h_value = internalPlanCostEstimator->compute_trusted_value_with_cache(s_i, &s_j);
                if (h_value == PlanCostEstimator::ReturnCode::DEAD_END) {
                    continue;
                }
            }
            int path_fragment_cost = 0;
            for (int k = path_start_index; k < path_target_index; ++k) {
                path_fragment_cost += action_costs[k];
            }
            max_value = std::max<int>(max_value, path_fragment_cost - h_value);
        }
    }
    return max_value;
}

bool loopinessBias::can_exclude_state(const State &s) {
    if (h) {
        EvaluationContext context(s);
        EvaluationResult result = h->compute_result(context);
        return result.is_infinite();
    } else {
        return false;
    }
}

static Plugin<FuzzingBias> _plugin_bias_loopinessBias("loopiness_bias", options::parse<PolicyBasedBias, loopinessBias>);
} // namespace policy_testing
