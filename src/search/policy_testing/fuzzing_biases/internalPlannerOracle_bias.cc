#include "internalPlannerOracle_bias.h"

#include "../plugin.h"

namespace policy_testing {
InternalPlannerOracleBias::InternalPlannerOracleBias(const options::Options &opts)
    : internalPlannerOracle(std::dynamic_pointer_cast<InternalPlannerPlanCostEstimator>(
                                opts.get<std::shared_ptr<PlanCostEstimator>>("internal_planner_oracle"))) {
    if (!internalPlannerOracle) {
        std::cerr << "You need to provide an InternalPlannerPlanCostEstimator" << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
    }
    register_sub_component(internalPlannerOracle.get());
}

void
InternalPlannerOracleBias::add_options_to_parser(options::OptionParser &parser) {
    parser.add_option<std::shared_ptr<PlanCostEstimator>>("internal_planner_oracle");
}

int
InternalPlannerOracleBias::bias(const State &state, unsigned int) {
    const int result = internalPlannerOracle->compute_trusted_value_with_cache(state);
    if (result == PlanCostEstimator::ReturnCode::DEAD_END) {
        return NEGATIVE_INFINITY;
    }
    return result;
}

bool InternalPlannerOracleBias::can_exclude_state(const State &s) {
    return internalPlannerOracle->compute_trusted_value_with_cache(s) == PlanCostEstimator::ReturnCode::DEAD_END;
}

static Plugin<FuzzingBias> _plugin_bias_internalPlannerOracleBias(
    "internal_planner_oracle_bias",
    options::parse<FuzzingBias, InternalPlannerOracleBias>);
} // namespace policy_testing
