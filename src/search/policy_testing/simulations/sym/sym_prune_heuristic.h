#pragma once

#include "sym_variables.h"
#include "../../../option_parser.h"

#include "../merge_and_shrink/abstraction_builder.h"
#include <memory>

namespace simulations {
class SymManager;
class LDSimulation;
class SymTransition;
class Abstraction;

class SymPruneHeuristic {
    OperatorCost cost_type;

    const bool prune_irrelevant;
    const bool dominance_pruning;

    std::unique_ptr<AbstractionBuilder> abstractionBuilder;
    std::unique_ptr<LDSimulation> ldSimulation;
    std::vector<std::unique_ptr<Abstraction>> abstractions;
    std::unique_ptr<SymTransition> tr;     //TR that computes dominated states

public:
    virtual void initialize(SymManager *mgr);

    BDD simulatedBy(const BDD &bdd);

    explicit SymPruneHeuristic(const options::Options &opts);

    virtual ~SymPruneHeuristic() = default;

    [[nodiscard]] bool use_dominance_pruning() const {
        return dominance_pruning;
    }
};
}
