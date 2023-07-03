#pragma once

#include <iostream>
#include <vector>
#include "labels.h"
#include "label.h"
#include "label_relation.h"

namespace simulations {
class EquivalenceRelation;
class LabelledTransitionSystem;
class SimulationRelation;
class DominanceRelation;

/*
 * Label relation represents the preorder relations on labels that
 * occur in a set of LTS
 */
class LabelRelationIdentity {
    [[maybe_unused]] Labels *labels; // TODO: currently unused private field
    int num_labels;

public:
    explicit LabelRelationIdentity(Labels *labels);

    void init(const std::vector<LabelledTransitionSystem *> & /*lts*/,
              const DominanceRelation & /*sim*/,
              const LabelMap & /*labelMap*/) {}

    void reset() {}

    bool update(const std::vector<LabelledTransitionSystem *> & /*lts*/,
                const DominanceRelation & /*sim*/) {return false;}

    void dump() const {}

    void dump(int /*label*/) const {}

    void dump_equivalent() const {}

    void dump_dominance() const {}


    [[nodiscard]] inline int get_num_labels() const {
        return num_labels;
    }

    [[nodiscard]] inline int get_dominated_by_noop_in(int /*l*/) const {
        return DOMINATES_IN_NONE;
    }

    [[nodiscard]] inline bool dominated_by_noop(int /*l*/, int /*lts*/) const {
        return false;
    }

    [[nodiscard]] inline bool dominates(int l1, int l2, int /*lts*/) const {
        return l1 == l2;
    }

    [[nodiscard]] std::vector<int> get_labels_dominated_in_all() const {
        return {};
    }

    void kill_label(int /*l*/) {}


    void prune_operators() {}

    //dangerousLTSs returns the set of LTSs where labels that dominate each other could not be included in the equivalence relation.
    EquivalenceRelation *
    get_equivalent_labels_relation(const LabelMap &labelMap, std::set<int> &dangerous_LTSs) const;

    [[nodiscard]] bool propagate_transition_pruning(int lts_id,
                                                    const std::vector<LabelledTransitionSystem *> &ltss,
                                                    const DominanceRelation &simulations,
                                                    int src, int l1, int target) const;
};
}
