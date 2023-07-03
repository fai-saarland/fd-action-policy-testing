#pragma once

#include "dominance_relation.h"
#include "labelled_transition_system.h"

namespace simulations {
class Labels;

/*
 * First implementation of a simulation relation.
 * THIS IMPLEMENTATION IS VERY INEFFICIENT
 * ONLY TO BE USED AS A PROOF OF CONCEPT
 */
template<typename LR>
class DominanceRelationSimple : public DominanceRelationLR<LR> {
public:
    template<typename LTS>
    static void
    update_sim(int lts_id, const LTS *lts, const LR &label_dominance,
               SimulationRelation &simrel);

    explicit DominanceRelationSimple(Labels *labels) : DominanceRelationLR<LR>(labels) {}

    virtual std::unique_ptr<SimulationRelation> init_simulation(Abstraction *_abs);

    virtual std::unique_ptr<SimulationRelation>
    init_simulation_incremental(CompositeAbstraction *_abs,
                                const SimulationRelation &simrel_one,
                                const SimulationRelation &simrel_two);

    /* bool propagate_label_domination(int lts_id, const LabelledTransitionSystem *lts,
                                    const LR &label_dominance, int l, int l2,
                                    SimulationRelation &simrel) const; */


    virtual void update(int lts_id, const LabelledTransitionSystem *lts,
                        const LR &label_dominance, SimulationRelation &simrel) {
        update_sim(lts_id, lts, label_dominance, simrel);
    }
};
}
