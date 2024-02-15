#pragma once

#include "../policy.h"

#include <memory>

class Evaluator;

namespace policy_testing {
/**
 * Extends the HeuristicDescendPolicy by running a BrFS to find a descendent
 * state with strictly smaller heuristic value. The found path is stored in the
 * policy cache so that sub-sequent apply calls will iteratively walk along this
 * path.
 **/
class HillClimbingPolicy : public Policy {
public:
    explicit HillClimbingPolicy(const plugins::Options &opts);
    static void add_options_to_feature(plugins::Feature &feature);

protected:
    OperatorID apply(const State &state) override;

private:
    std::shared_ptr<Evaluator> heuristic_;
    const bool helpful_actions_pruning_;
};
} // namespace policy_testing
