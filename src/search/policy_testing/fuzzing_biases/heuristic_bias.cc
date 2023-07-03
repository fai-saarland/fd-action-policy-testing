#include "heuristic_bias.h"
#include "../plugin.h"

namespace policy_testing {
heuristicBias::heuristicBias(const options::Options &opts)
    : heuristic(opts.get<std::shared_ptr<Evaluator>>("h")) {
}

void
heuristicBias::add_options_to_parser(options::OptionParser &parser) {
    parser.add_option<std::shared_ptr<Evaluator>>("h",
                                                  "Heuristic Bias is only implemented for safe heuristics, i.e., if the heuristic returns infinity a bias of negative infinity will be chosen.");
}

int
heuristicBias::bias(const State &state, unsigned int) {
    EvaluationContext context(state);
    EvaluationResult result = heuristic->compute_result(context);
    if (result.is_infinite()) {
        return NEGATIVE_INFINITY;
    }
    return result.get_evaluator_value();
}

bool heuristicBias::can_exclude_state(const State &s) {
    EvaluationContext context(s);
    EvaluationResult result = heuristic->compute_result(context);
    return result.is_infinite();
}

static Plugin<FuzzingBias> _plugin_bias_heuristic(
    "heuristic_bias",
    options::parse<FuzzingBias, heuristicBias>);
} // namespace policy_testing
