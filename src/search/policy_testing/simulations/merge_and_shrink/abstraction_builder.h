#pragma once

#include "ld_simulation.h"
#include "../../../operator_cost.h"
#include "../../../plugins/plugin.h"

#include <memory>

namespace simulations {
class Labels;
class Abstraction;
class MergeStrategy;
class ShrinkStrategy;

class AbstractionBuilder {
    const plugins::Options opts;
protected:
    const bool expensive_statistics;
    const bool dump;

    const int limit_seconds_total;
    const int limit_memory_kb_total; //Limit of seconds for building the abstraction

public:
    explicit AbstractionBuilder(const plugins::Options &opts);
    virtual ~AbstractionBuilder() = default;

    void  init_ldsim(bool unit_cost, OperatorCost cost_type,
                     std::unique_ptr <LDSimulation> &ldSim) const;

    virtual void build_abstraction(bool unit_cost, OperatorCost cost_type,
                                   std::unique_ptr<LDSimulation> &ldSim,
                                   std::vector<std::unique_ptr<Abstraction>> &abstractions) const = 0;

    // TODO adapt if OperatorCost::Zero is supported
    std::unique_ptr<LDSimulation>
    build_abstraction(OperatorCost cost_type, std::vector<std::unique_ptr<Abstraction>> &abstractions) const {
        std::unique_ptr<LDSimulation> ldSim;
        build_abstraction(is_unit_cost_task(cost_type), cost_type, ldSim, abstractions);
        return ldSim;
    }

    virtual void dump_options() const = 0;

    static void add_options_to_feature(plugins::Feature &feature);
};


class AbsBuilderPDB : public AbstractionBuilder {
    const int limit_absstates_merge;

public:
    explicit AbsBuilderPDB(const plugins::Options &opts);
    ~AbsBuilderPDB() override = default;

    void build_abstraction(bool unit_cost, OperatorCost cost_type,
                           std::unique_ptr<LDSimulation> &ldSim,
                           std::vector<std::unique_ptr<Abstraction>> &abstractions) const override;

    void dump_options() const override {
        std::cout << "AbsBuilderPDB" << std::endl;
    }
};

class AbsBuilderAtomic : public AbstractionBuilder {
public:
    explicit AbsBuilderAtomic(const plugins::Options &opts);
    ~AbsBuilderAtomic() override = default;


    void build_abstraction(bool unit_cost, OperatorCost cost_type,
                           std::unique_ptr<LDSimulation> &ldSim,
                           std::vector<std::unique_ptr<Abstraction>> &abstractions) const override;

    void dump_options() const override {
        std::cout << "AbsBuilderAtomic" << std::endl;
    }
};


class AbsBuilderMAS : public AbstractionBuilder {
    std::shared_ptr<MergeStrategy> merge_strategy;
    std::shared_ptr<ShrinkStrategy> shrink_strategy;
    const bool shrink_after_merge;

    const int limit_seconds_mas; //Limit of seconds for building the abstraction

    bool prune_dead_operators;
    bool store_original_operators;

    const bool restart;
    const int num_abstractions;

public:
    explicit AbsBuilderMAS(const plugins::Options &opts);
    ~AbsBuilderMAS() override = default;

    void build_abstraction(bool unit_cost, OperatorCost cost_type,
                           std::unique_ptr<LDSimulation> &ldSim,
                           std::vector<std::unique_ptr<Abstraction>> &abstractions) const override;

    void dump_options() const override {
        std::cout << "AbsBuilderMAS" << std::endl;
    }
};



class AbsBuilderDefault : public AbstractionBuilder {
    std::shared_ptr<MergeStrategy> merge_strategy;
    const bool original_merge;
    const int limit_absstates_merge;
    const int min_limit_absstates_merge;
    const int limit_transitions_merge;

    const int limit_absstates_shrink;

    const int limit_seconds_mas; //Limit of seconds for building the abstraction

    const int num_abstractions;
    const int switch_off_label_dominance;

public:
    explicit AbsBuilderDefault(const plugins::Options &opts);
    ~AbsBuilderDefault() override = default;

    void build_abstraction(bool unit_cost, OperatorCost cost_type,
                           std::unique_ptr<LDSimulation> &ldSim,
                           std::vector<std::unique_ptr<Abstraction>> &abstractions) const override;

    void dump_options() const override {
        std::cout << "AbsBuilderDefault" << std::endl;
    }
};



class AbsBuilderMasSimulation : public AbstractionBuilder {
    const SimulationType simulation_type;
    const LabelDominanceType label_dominance_type;
    /* //Allows to switch off label dominance from normal to noop if the */
    /* //number of labels is greater than this parameter. */
    const int switch_off_label_dominance;

    const bool apply_simulation_shrinking;
    bool apply_subsumed_transitions_pruning;
    const bool apply_label_dominance_reduction;
    bool prune_dead_operators;
    bool store_original_operators;

    const bool complex_lts;
    /* const bool use_mas; */
    /* /\* Parameters for constructing a final abstraction after the simulation *\/ */
    /* const bool compute_final_abstraction;  */

    std::shared_ptr<MergeStrategy> merge_strategy;
    const bool original_merge;
    const int limit_absstates_merge;
    const int min_limit_absstates_merge;
    const int limit_transitions_merge;

    bool intermediate_simulations;
    bool incremental_simulations;
    const bool compute_final_simulation;

    const bool forbid_lr;

    std::shared_ptr<ShrinkStrategy> shrink_strategy;
    const bool shrink_after_merge;

    const int limit_seconds_mas; //Limit of seconds for building the abstraction

    // void compute_ld_simulation(LDSimulation &ldSim, bool incremental_step = false) const;

public:
    explicit AbsBuilderMasSimulation(const plugins::Options &opts);
    ~AbsBuilderMasSimulation() override = default;

    void build_abstraction(bool unit_cost, OperatorCost cost_type,
                           std::unique_ptr<LDSimulation> &ldSim,
                           std::vector<std::unique_ptr<Abstraction>> &abstractions) const override;

    void dump_options() const override;
};



class AbsBuilderComposite : public AbstractionBuilder {
    const std::vector<AbstractionBuilder *> strategies;
public:
    explicit AbsBuilderComposite(const plugins::Options &opts);
    ~AbsBuilderComposite() override = default;

    void build_abstraction(bool unit_cost, OperatorCost cost_type,
                           std::unique_ptr<LDSimulation> &ldSim,
                           std::vector<std::unique_ptr<Abstraction>> &abstractions) const override;

    void dump_options() const override {
        for (auto st : strategies) {
            std::cout << "  ST1: ";
            st->dump_options();
        }
    }
};
}
