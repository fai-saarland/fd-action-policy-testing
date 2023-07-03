#include "dominance_bias.h"
#include "../plugin.h"
#include "../../evaluation_context.h"

namespace policy_testing {
DominanceBias::DominanceBias(const options::Options &opts) :
    PolicyBasedBias(opts),
    numericDominanceOracle(std::dynamic_pointer_cast<NumericDominanceOracle>(
                               opts.get<std::shared_ptr<Oracle>>("numeric_dominance_oracle"))) {
    if (!numericDominanceOracle) {
        std::cerr << "Dominance bias needs to be set up with a NumericDominanceOracle" << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
    }
    assert(numericDominanceOracle);
    register_sub_component(numericDominanceOracle.get());
}

void
DominanceBias::add_options_to_parser(options::OptionParser &parser) {
    parser.add_option<std::shared_ptr<Oracle>>("numeric_dominance_oracle",
                                               "Numeric dominance relation (will be used to read the dominance function)");
    PolicyBasedBias::add_options_to_parser(parser);
}

int
DominanceBias::bias(const State &state, unsigned int budget) {
    const std::vector<State> path = policy->execute_get_path_fragment(state, get_step_limit(budget), false);
    const std::vector<int> action_costs = policy->read_path_action_costs(path);
    const int lower_finite_dominance_bound = numericDominanceOracle->minimal_finite_dominance_value;
    int max_value = lower_finite_dominance_bound;
    for (int i = 0; i < path.size() - 1; ++i) {
        const State &s_i = path[i];
        int path_fragment_cost = 0; // the cost between s_i and s_j
        for (int j = i + 1; j < path.size(); ++j) {
            const State &s_j = path[j];
            path_fragment_cost += action_costs[j - 1];
            const int dominance_value = numericDominanceOracle->D(s_j, s_i);
            if (dominance_value == simulations::MINUS_INFINITY) {
                continue;
            } else if (path_fragment_cost + dominance_value > 0) {
                return POSITIVE_INFINITY;
            } else {
                max_value = std::max<int>(max_value, path_fragment_cost + dominance_value);
            }
        }
    }
    assert(max_value <= 0);
    assert(lower_finite_dominance_bound <= max_value);
    const int shift_value = -lower_finite_dominance_bound;
    return max_value + shift_value;
}

static Plugin<FuzzingBias> _plugin_bias_surfaceBias("dominance_bias", options::parse<PolicyBasedBias, DominanceBias>);
} // namespace policy_testing
