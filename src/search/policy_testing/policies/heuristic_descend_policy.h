#pragma once

#include "../policy.h"

#include <memory>

class Evaluator;

namespace policy_testing {
/**
 * Chooses the action leading to a successor state with minimal heuristic
 * value. If strictly_descend is set and the minimal successor heuristic
 * value is not strictly smaller than the state's heuristic value, return
 * NO_OPERATOR.
 **/
class HeuristicDescendPolicy : public Policy {
public:
    explicit HeuristicDescendPolicy(const plugins::Options &opts);
    static void add_options_to_feature(plugins::Feature &feature);

protected:
    OperatorID apply(const State &state) override;

private:
    std::shared_ptr<Evaluator> heuristic_;
    const bool strictly_descend_;
    const bool stop_at_dead_ends_;
};
} // namespace policy_testing
