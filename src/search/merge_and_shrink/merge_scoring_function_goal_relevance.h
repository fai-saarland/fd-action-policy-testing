#ifndef MERGE_AND_SHRINK_MERGE_SCORING_FUNCTION_GOAL_RELEVANCE_H
#define MERGE_AND_SHRINK_MERGE_SCORING_FUNCTION_GOAL_RELEVANCE_H

#include "merge_scoring_function.h"

namespace merge_and_shrink {
class TransitionSystem;
class MergeScoringFunctionGoalRelevance : public MergeScoringFunction {
protected:
virtual std::string name() const override;
public:
    MergeScoringFunctionGoalRelevance() = default;
    virtual std::vector<int> compute_scores(
        FactoredTransitionSystem &fts,
        const std::vector<std::pair<int, int>> &merge_candidates) override;
};
}

#endif
