#include "label_relation_identity.h"

#include "labels.h"
#include "dominance_relation.h"
#include "labelled_transition_system.h"
#include "../utils/equivalence_relation.h"

namespace simulations {
LabelRelationIdentity::LabelRelationIdentity(Labels *_labels) : labels(_labels),
                                                                num_labels(_labels->get_size()) {
}

EquivalenceRelation *
LabelRelationIdentity::get_equivalent_labels_relation(const LabelMap &labelMap,
                                                      std::set<int> & /*dangerous_LTSs*/) const {
    std::list <Block> rel;
    for (int l1 = 0; l1 < num_labels; l1++) {
        Block eq;
        eq.insert(labelMap.get_old_id(l1));
        rel.push_back(eq);
    }

    return new EquivalenceRelation(rel.size(), rel);
}


/* Returns true if we succeeded in propagating the effects of pruning a transition in lts i. */
bool LabelRelationIdentity::propagate_transition_pruning(int lts_id,
                                                         const std::vector<LabelledTransitionSystem *> &ltss,
                                                         const DominanceRelation & /*simulations*/,
                                                         int src, int l1, int target) {
    ltss[lts_id]->kill_transition(src, l1, target);
    return true;
}
}
