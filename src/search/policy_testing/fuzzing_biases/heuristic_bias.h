#include "../engines/pool_fuzzer.h"

#include "../../heuristics/ff_heuristic.h"
#include "../../evaluator.h"
#include "../../evaluation_context.h"
#include "../fuzzing_bias.h"

namespace policy_testing {
class heuristicBias : public FuzzingBias {
public:
    explicit heuristicBias(const plugins::Options &opts);
    static void add_options_to_feature(plugins::Feature &feature);
    int bias(const State &state, unsigned int budget) override;
    bool can_exclude_state(const State &s) override;

private:
    // Heuristic Bias is only implemented for safe heuristics,
    // i.e., if heuristic is infinity, we choose a bias of negative infinity.
    std::shared_ptr<Evaluator> heuristic;
};
} //namespace policy_testing
