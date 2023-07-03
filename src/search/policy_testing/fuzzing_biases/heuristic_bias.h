#include "../engines/pool_fuzzer.h"

#include "../../heuristics/ff_heuristic.h"
#include "../../evaluator.h"
#include "../../evaluation_context.h"
#include "../../option_parser.h"
#include "../fuzzing_bias.h"

namespace policy_testing {
class heuristicBias : public FuzzingBias {
public:
    explicit heuristicBias(const options::Options &opts);
    static void add_options_to_parser(options::OptionParser &parser);
    int bias(const State &state, unsigned int budget) override;
    bool can_exclude_state(const State &s) override;

private:
    // Heuristic Bias is only implemented for safe heuristics, i.e., if the heuristic returns infinity a bias of negative infinity will be chosen.
    std::shared_ptr<Evaluator> heuristic;
};
} //namespace policy_testing
