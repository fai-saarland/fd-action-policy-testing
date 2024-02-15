#include "simulation_simple.h"

#include "simulation_relation.h"
#include "labelled_transition_system.h"
#include "label_relation.h"
#include "alternative_label_relation.h"
#include "label_relation_identity.h"
#include "label_relation_noop.h"
#include "abstraction.h"


namespace simulations {
template<typename LR>
template<typename LTS> void
DominanceRelationSimple<LR>::update_sim(int lts_id, const LTS *lts,
                                        const LR &label_dominance,
                                        SimulationRelation &simrel) {
    bool changes = true;
    while (changes) {
        changes = false;
        for (int s = 0; s < lts->size(); s++) {
            for (int t = 0; t < lts->size(); t++) {     //for each pair of states t, s
                if (s != t && simrel.simulates(t, s) && !simrel.fixed_simulates(t, s)) {
                    //cout << "Checking states " << lts->name(s) << " and " << lts->name(t) << endl;
                    //Check if really t simulates s
                    //for each transition s--l->s':
                    // a) with noop t >= s' and l dominated by noop?
                    // b) exist t--l'-->t', t' >= s' and l dominated by l'?
                    lts->applyPostSrc(s, [&](const LTSTransition &trs) {
                                          const std::vector<int> &labels_trs = lts->get_labels(trs.label_group);
                                          for (int label : labels_trs) {
                                              //cout << "Checking transition " << trs.label << " " << g_operators[trs.label].get_name() << " to " << trs.target << endl;
                                              if (simrel.simulates(t, trs.target)) {
                                                  if (label_dominance.dominated_by_noop(label, lts_id)) {
                                                      continue;
                                                  }
                                              }
                                              bool found =
                                                  lts->applyPostSrc(t, [&](const LTSTransition &trt) {
                                                                        if (simrel.simulates(trt.target, trs.target)) {
                                                                            const std::vector<int> &labels_trt =
                                                                                lts->get_labels(trt.label_group);
                                                                            for (int label_trt : labels_trt) {
                                                                                if (label_dominance.dominates(label_trt,
                                                                                                              label,
                                                                                                              lts_id)) {
                                                                                    return true;
                                                                                }
                                                                            }
                                                                        }
                                                                        return false;
                                                                    });

                                              if (!found) {
                                                  changes = true;
                                                  simrel.remove(t, s);
                                                  return true;
                                              }
                                          }

                                          return false;
                                      });
                }
            }
        }
    }
}

template<typename LR>
std::unique_ptr<SimulationRelation> DominanceRelationSimple<LR>::init_simulation(Abstraction *_abs) {
    std::unique_ptr<SimulationRelation> res(new SimulationRelation(_abs));
    res->init_goal_respecting();
    return res;
}

template<typename LR>
std::unique_ptr<SimulationRelation>
DominanceRelationSimple<LR>::init_simulation_incremental(CompositeAbstraction *_abs,
                                                         const SimulationRelation &simrel_one,
                                                         const SimulationRelation &simrel_two) {
    std::unique_ptr<SimulationRelation> res(new SimulationRelation(_abs));
    res->init_incremental(_abs, simrel_one, simrel_two);
    return res;
}


template
class DominanceRelationSimple<LabelRelation>;

template
class DominanceRelationSimple<LabelRelationIdentity>;

template
class DominanceRelationSimple<LabelRelationNoop>;

template class DominanceRelationSimple<AlternativeLabelRelation>;
}
