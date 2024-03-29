#ifndef SEARCH_ALGORITHMS_ENFORCED_HILL_CLIMBING_SEARCH_H
#define SEARCH_ALGORITHMS_ENFORCED_HILL_CLIMBING_SEARCH_H

#include "../evaluation_context.h"
#include "../open_list.h"
#include "../search_algorithm.h"

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

namespace plugins {
class Options;
}

namespace enforced_hill_climbing_search {
enum class PreferredUsage {
    PRUNE_BY_PREFERRED,
    RANK_PREFERRED_FIRST
};

class InitException : public std::exception {
    // exception to be thrown when engine initialization would otherwise be terminated with utils::exit_with
};

/*
  Enforced hill-climbing with deferred evaluation.

  TODO: We should test if this lazy implementation really has any benefits over
  an eager one. We hypothesize that both versions need to evaluate and store
  the same states anyways.
*/
class EnforcedHillClimbingSearch : public SearchAlgorithm {
    std::unique_ptr<EdgeOpenList> open_list;

    std::shared_ptr<Evaluator> evaluator;
    std::vector<std::shared_ptr<Evaluator>> preferred_operator_evaluators;
    std::set<Evaluator *> path_dependent_evaluators;
    bool use_preferred;
    PreferredUsage preferred_usage;

    EvaluationContext current_eval_context;
    int current_phase_start_g;

    // Statistics
    std::map<int, std::pair<int, int>> d_counts;
    int num_ehc_phases;
    int last_num_expanded;

    const bool prevent_exit;

    void insert_successor_into_open_list(
        const EvaluationContext &eval_context,
        int parent_g,
        OperatorID op_id,
        bool preferred);
    void expand(EvaluationContext &eval_context);
    void reach_state(
        const State &parent, OperatorID op_id, const State &state);
    SearchStatus ehc();

protected:
    virtual void initialize() override;
    virtual SearchStatus step() override;

public:
    explicit EnforcedHillClimbingSearch(const plugins::Options &opts);
    virtual ~EnforcedHillClimbingSearch() override;

    virtual void print_statistics() const override;
};
}

#endif
