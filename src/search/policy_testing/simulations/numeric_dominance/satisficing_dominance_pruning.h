#pragma once

// #include "../prune_heuristic.h"
#include "../sym/sym_variables.h"
#include "../sym/sym_params.h"

#include "int_epsilon.h"
#include "satisficing_dominance_relation.h"
#include "tau_labels.h"

namespace simulations {
class LDSimulation;
class AbstractionBuilder;
class SymVariables;
class SymManager;
class Abstraction;

// class SatisficingDominancePruning : public PruneHeuristic {
class SatisficingDominancePruning {
    // TODO refactor
    OperatorCost cost_type;
protected:
    bool initialized;
    std::shared_ptr<TauLabelManager<int>> tau_label_mgr;
    const bool use_quantified_dominance;
    const bool trade_off_dominance;
    const bool only_positive_dominance;

    const bool prune_dominated_by_parent;
    const bool prune_dominated_by_initial_state;
    const bool prune_successors;

    const int truncate_value;
    const int max_simulation_time;
    const int min_simulation_time;
    const int max_total_time;
    const int max_lts_size_to_compute_simulation;
    const int num_labels_to_use_dominates_in;

    const bool dump;
    const bool exit_after_preprocessing;

    std::unique_ptr<AbstractionBuilder> abstractionBuilder;
    std::unique_ptr<LDSimulation> ldSimulation;
    std::unique_ptr<SatisficingDominanceRelation> numeric_dominance_relation;
    std::vector<std::unique_ptr<Abstraction>> abstractions;

    void dump_options() const;

    [[nodiscard]] bool apply_pruning() const;

public:

    explicit SatisficingDominancePruning(const options::Options &opts);

    virtual ~SatisficingDominancePruning() = default;


    //virtual void initialize(bool force_initialization = false) override;
    void initialize(bool force_initialization = false);

    /*
    virtual bool prune_generation(const State &, int, const State &, int ) override {
        return false;
    }
    virtual bool prune_expansion (const State &, int ) override {
        return false;
    }
    virtual bool proves_task_unsolvable() const override {
        return true;
    }
     */

    //Methods for pruning explicit search
    /*
    virtual void prune_applicable_operators(const State & state, int g,
                        std::vector<const Operator *> & operators,
                        SearchProgress & search_progress) override;
    virtual bool is_dead_end(const State &state) override;
     */

    //virtual int compute_heuristic(const State &state) override;
    //  int compute_heuristic(const State &state);

    // virtual void print_statistics() override {}

    //virtual bool strictly_dominates (const State &, const State &) const override;

    //virtual void restart_search(const State & new_initial_state) override;

    // virtual bool strictly_dominates_initial_state (const State & new_state) const override;
};
}
