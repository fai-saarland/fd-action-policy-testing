#include "label_relation.h"

#include "labels.h"
#include "simulation_relation.h"
#include "dominance_relation.h"
#include "labelled_transition_system.h"
#include "../utils/equivalence_relation.h"
#include "../utils/debug.h"

#include "../numeric_dominance/satisficing_dominance_relation.h"

namespace simulations {
LabelRelation::LabelRelation(Labels *_labels) : labels(_labels),
                                                num_labels(_labels->get_size()) {
}

void LabelRelation::dump_equivalent() const {
    std::vector<bool> redundant(dominates_in.size(), false);
    int num_redundant = 0;
    for (int l1 = 0; l1 < dominates_in.size(); ++l1) {
        for (int l2 = l1 + 1; l2 < dominates_in.size(); ++l2) {
            if (!redundant[l2] && dominates_in[l1][l2] != DOMINATES_IN_NONE &&
                dominates_in[l2][l1] == dominates_in[l1][l2]) {
                redundant[l2] = true;
                num_redundant++;
                std::cout << l1 << " equivalent to " << l2 << " in " << dominates_in[l1][l2] << std::endl;
            }
        }
    }
    std::cout << "Redundant labels: " << num_redundant << std::endl;
}


void LabelRelation::dump_dominance() const {
    for (int l1 = 0; l1 < dominates_in.size(); ++l1) {
        for (int l2 = 0; l2 < dominates_in.size(); ++l2) {
            if (dominates_in[l1][l2] != DOMINATES_IN_NONE &&
                dominates_in[l2][l1] != dominates_in[l1][l2]) {
                std::cout << l1 << " dominates " << l2 << " in " << dominates_in[l1][l2] << std::endl;
                std::cout << global_simulation_task()->get_operator_name(l1, false) << " dominates "
                          << global_simulation_task()->get_operator_name(l2, false) << std::endl;
            }
        }
    }
}

void LabelRelation::dump() const {
    for (int l = 0; l < dominates_in.size(); ++l) {
        //if (labels->is_label_reduced(l)) cout << "reduced";
        if (l < 10) {
            std::cout << "l" << l << ": ";
            dump(l);
        } else {
            std::cout << "l" << l << ":";
            dump(l);
        }
    }
}

void LabelRelation::dump(int label) const {
    std::cout << "Dump l: " << label << "; ";
    if (dominated_by_noop_in[label] >= 0 && dominated_by_noop_in[label] <= 9) {
        std::cout << " Dominated by noop: " << dominated_by_noop_in[label] << ", labels: ";
    } else {
        std::cout << " Dominated by noop:" << dominated_by_noop_in[label] << ", labels: ";
    }

    for (int l2 = 0; l2 < dominates_in[label].size(); ++l2) {
        if (dominates_in[l2][label] >= 0 && dominates_in[l2][label] <= 9)
            std::cout << " ";
        std::cout << dominates_in[l2][label] << " ";
    }
    std::cout << std::endl;
}

void LabelRelation::prune_operators() {
    //cout << "We have " << num_labels << " labels "<< dominates_in.size() << " " << dominates_in[0].size()  << endl;
    for (int l = 0; l < dominates_in.size(); ++l) {
        //labels->get_label_by_index(l)->dump();
        if (dominated_by_noop_in[l] == DOMINATES_IN_ALL) {
            std::cout << global_simulation_task()->get_operator_name(l, false) << " is dominated by noop "
                      << std::endl;
        }

        for (int l2 = 0; l2 < dominates_in.size(); ++l2) {
            if (l2 != l && dominates_in[l2][l] == DOMINATES_IN_ALL) {
                std::cout << global_simulation_task()->get_operator_name(l, false) << " is dominated by "
                          << global_simulation_task()->get_operator_name(l2, false) << std::endl;
            }
        }
    }
}

std::vector<int> LabelRelation::get_labels_dominated_in_all() const {
    std::vector<int> labels_dominated_in_all;
    //cout << "We have " << num_labels << " labels "<< dominates_in.size() << " " << dominates_in[0].size()  << endl;
    for (int l = 0; l < dominates_in.size(); ++l) {
        //cout << "Check: " << l << endl;
        //labels->get_label_by_index(l)->dump();
        if (dominated_by_noop_in[l] == DOMINATES_IN_ALL) {
            labels_dominated_in_all.push_back(l);
            continue;
        }

        // PIET-edit: Here we remove one of the two labels dominating each other in all LTSs.
        for (int l2 = 0; l2 < dominates_in.size(); ++l2) {
            if ((l2 < l && dominates_in[l2][l] == DOMINATES_IN_ALL &&
                 dominates_in[l][l2] != DOMINATES_IN_ALL)
                || (l2 > l && dominates_in[l2][l] == DOMINATES_IN_ALL)) {
                labels_dominated_in_all.push_back(l);
                break;
            }
        }
    }

    return labels_dominated_in_all;
}

void LabelRelation::reset() {
    std::cout << "Error: reset of label relation has been disabled" << std::endl;
    exit(0);
}

void LabelRelation::init(const std::vector<LabelledTransitionSystem *> &lts,
                         const DominanceRelation &sim,
                         const LabelMap &labelMap) {
    num_labels = labelMap.get_num_labels();

    //TODO: Only do this in the incremental step (reset)
    //TODO: Is there a better way to reinitialize these?
    std::vector<std::vector<bool>>().swap(simulates_irrelevant);
    std::vector<std::vector<bool>>().swap(simulated_by_irrelevant);
    std::vector<std::vector<int>>().swap(dominates_in);
    std::vector<int>().swap(dominated_by_noop_in);

    simulates_irrelevant.resize(num_labels);
    simulated_by_irrelevant.resize(num_labels);
    for (int i = 0; i < num_labels; i++) {
        simulates_irrelevant[i].resize(lts.size(), true);
        simulated_by_irrelevant[i].resize(lts.size(), true);
    }

    dominates_in.resize(num_labels);
    dominated_by_noop_in.resize(num_labels, DOMINATES_IN_ALL);
    for (int l1 = 0; l1 < dominates_in.size(); ++l1) {
        int old_l1 = labelMap.get_old_id(l1);
        dominates_in[l1].resize(num_labels, DOMINATES_IN_ALL);
        for (int l2 = 0; l2 < dominates_in[l1].size(); ++l2) {
            int old_l2 = labelMap.get_old_id(l2);
            if (labels->get_label_by_index(old_l1)->get_cost() >
                labels->get_label_by_index(old_l2)->get_cost()) {
                dominates_in[l1][l2] = DOMINATES_IN_NONE;
            }
        }
    }
    DEBUG_MSG(std::cout << "Update label dominance: " << num_labels
                        << " labels " << lts.size() << " systems." << std::endl;
              );

    for (int i = 0; i < lts.size(); ++i) {
        update(i, lts[i], sim[i]);
    }
}


bool LabelRelation::update(const std::vector<LabelledTransitionSystem *> &lts,
                           const DominanceRelation &sim) {
    bool changes = false;

    for (int i = 0; i < lts.size(); ++i) {
        changes |= update(i, lts[i], sim[i]);
    }

    return changes;
}


void LabelRelation::init(const std::vector<LabelledTransitionSystem *> &lts,
                         const SatisficingDominanceRelation &sim,
                         const LabelMap &labelMap) {
    num_labels = labelMap.get_num_labels();

    //TODO: Only do this in the incremental step (reset)
    //TODO: Is there a better way to reinitialize these?
    std::vector<std::vector<bool>>().swap(simulates_irrelevant);
    std::vector<std::vector<bool>>().swap(simulated_by_irrelevant);
    std::vector<std::vector<int>>().swap(dominates_in);
    std::vector<int>().swap(dominated_by_noop_in);

    simulates_irrelevant.resize(num_labels);
    simulated_by_irrelevant.resize(num_labels);
    for (int i = 0; i < num_labels; i++) {
        simulates_irrelevant[i].resize(lts.size(), true);
        simulated_by_irrelevant[i].resize(lts.size(), true);
    }

    dominates_in.resize(num_labels);
    dominated_by_noop_in.resize(num_labels, DOMINATES_IN_ALL);
    for (int l1 = 0; l1 < dominates_in.size(); ++l1) {
        int old_l1 = labelMap.get_old_id(l1);
        dominates_in[l1].resize(num_labels, DOMINATES_IN_ALL);
        for (int l2 = 0; l2 < dominates_in[l1].size(); ++l2) {
            int old_l2 = labelMap.get_old_id(l2);
            if (labels->get_label_by_index(old_l1)->get_cost() >
                labels->get_label_by_index(old_l2)->get_cost()) {
                dominates_in[l1][l2] = DOMINATES_IN_NONE;
            }
        }
    }
    DEBUG_MSG(std::cout << "Update label dominance: " << num_labels
                        << " labels " << lts.size() << " systems." << std::endl;
              );

    update(lts, sim);
}


bool LabelRelation::update(const std::vector<LabelledTransitionSystem *> &lts,
                           const SatisficingDominanceRelation &sim) {
    bool changes = false;

    for (int i = 0; i < lts.size(); ++i) {
        changes |= update(i, lts[i], sim.get_boolean_simulation(i));
    }

    return changes;
}

/* TODO: This version is inefficient. It could be improved by
 * iterating only the right transitions (see TODO inside the loop)
 */
bool LabelRelation::update(int i, const LabelledTransitionSystem *lts,
                           const SimulationRelation &sim) {
    bool changes = false;
    for (int l2: lts->get_relevant_labels()) {
        for (int l1: lts->get_relevant_labels()) {
            if (l1 != l2 && simulates(l1, l2, i)) {
                //std::cout << "Check " << l1 << " " << l2 << std::endl;
                //std::cout << "Num transitions: " << lts->get_transitions_label(l1).size()
                //		    << " " << lts->get_transitions_label(l2).size() << std::endl;
                //Check if it really simulates
                //For each transition s--l2-->t, and every label l1 that dominates
                //l2, exist s--l1-->t', t <= t'?
                for (const auto &tr: lts->get_transitions_label(l2)) {
                    bool found = false;
                    //TODO: for(auto tr2 : lts->get_transitions_for_label_src(l1, tr.src)){
                    for (const auto &tr2: lts->get_transitions_label(l1)) {
                        if (tr2.src == tr.src &&
                            sim.simulates(tr2.target, tr.target)) {
                            found = true;
                            break;     //Stop checking this tr
                        }
                    }
                    if (!found) {
                        //std::cout << "Not sim " << l1 << " " << l2 << " " << i << std::endl;
                        set_not_simulates(l1, l2, i);
                        changes = true;
                        break;     //Stop checking trs of l1
                    }
                }
            }
        }

        //Is l2 simulated by irrelevant_labels in lts?
        for (auto tr: lts->get_transitions_label(l2)) {
            if (simulated_by_irrelevant[l2][i] &&
                !sim.simulates(tr.src, tr.target)) {
                changes |= set_not_simulated_by_irrelevant(l2, i);
                for (int l: lts->get_irrelevant_labels()) {
                    if (simulates(l, l2, i)) {
                        changes = true;
                        set_not_simulates(l, l2, i);
                    }
                }
                break;
            }
        }

        //Does l2 simulates irrelevant_labels in lts?
        if (simulates_irrelevant[l2][i]) {
            for (int s = 0; s < lts->size(); s++) {
                bool found = false;
                for (const auto &tr: lts->get_transitions_label(l2)) {
                    if (tr.src == s && sim.simulates(tr.target, tr.src)) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    simulates_irrelevant[l2][i] = false;
                    for (int l: lts->get_irrelevant_labels()) {
                        if (simulates(l2, l, i)) {
                            set_not_simulates(l2, l, i);
                            changes = true;
                        }
                    }
                    break;
                }
            }
        }
    }

    return changes;
}

EquivalenceRelation *LabelRelation::get_equivalent_labels_relation(const LabelMap &labelMap,
                                                                   std::set<int> &dangerous_LTSs)
const {
    std::list<Block> rel;
    std::vector<int> captured_labels(num_labels, -1);
    std::vector<int> Theta(num_labels, DOMINATES_IN_ALL);     //LTS in which we are aggregating each label
    for (int l1 = 0; l1 < num_labels; l1++) {
        Block eq;
        if (captured_labels[l1] == -1) {
            captured_labels[l1] = l1;
            eq.insert(labelMap.get_old_id(l1));
        }
        for (int l2 = l1 + 1; l2 < num_labels; l2++) {
            // was already marked as equivalent to some other label, so cannot be equivalent to l1
            if (dominates_in[l1][l2] != DOMINATES_IN_NONE &&
                dominates_in[l2][l1] != DOMINATES_IN_NONE &&
                (dominates_in[l1][l2] == DOMINATES_IN_ALL ||
                 dominates_in[l2][l1] == DOMINATES_IN_ALL ||
                 dominates_in[l1][l2] == dominates_in[l2][l1])) {
                int new_Theta =
                    dominates_in[l1][l2] == DOMINATES_IN_ALL ? dominates_in[l2][l1] : dominates_in[l1][l2];

                if (new_Theta == DOMINATES_IN_ALL ||
                    ((Theta[l2] == DOMINATES_IN_ALL || Theta[l2] == new_Theta)
                     && (Theta[l1] == DOMINATES_IN_ALL || Theta[l1] == new_Theta))) {
                    // we can insert l2 into rel
                    if (new_Theta != DOMINATES_IN_ALL) {
                        //Assign new_Theta as the only LTS where l2 and l1 can be aggregated with other labels
                        Theta[l2] = new_Theta;
                        Theta[l1] = new_Theta;
                    }

                    if (captured_labels[l2] == -1) {
                        eq.insert(labelMap.get_old_id(l2));
                        //cout << labelMap.get_old_id(l2) << " eq " << labelMap.get_old_id(l1) << endl;
                        //cout << dominates_in[l1][l2] << " --- " <<  dominates_in[l2][l1] << " --- " << endl;
                        captured_labels[l2] = l1;
                    } else if (captured_labels[l2] != captured_labels[l1]) {
                        std::cout
                            << "Assertion Error: two labels are aggregated but they were already aggregated before?"
                            << std::endl;
                        exit_with(EXIT_CRITICAL_ERROR);
                    }
                } else if (new_Theta != DOMINATES_IN_ALL) {
                    //cout << "eq skipped because is dangerous: " <<
                    //labelMap.get_old_id(l2) << " eq " << labelMap.get_old_id(l1) << endl
                    // << dominates_in[l1][l2] << " --- " <<  dominates_in[l2][l1] << " --- " << endl;
                    dangerous_LTSs.insert(new_Theta);
                } else {
                    std::cout << "Assertion Error: two labels dominate in all but cannot be aggregated?" << std::endl;
                    exit_with(EXIT_CRITICAL_ERROR);
                }
            }
        }

        rel.push_back(eq);
    }

    return new EquivalenceRelation(rel.size(), rel);
}


/* Returns true if we succeeded in propagating the effects of pruning a transition in lts i. */
bool LabelRelation::propagate_transition_pruning(int lts_id,
                                                 const std::vector<LabelledTransitionSystem *> &ltss,
                                                 const DominanceRelation &simulations,
                                                 int src, int l1, int target) const {
    LabelledTransitionSystem *lts = ltss[lts_id];
    const SimulationRelation &sim = simulations[lts_id];

    std::vector<bool> Tlbool(lts->size(), false), Tlpbool(lts->size(), false);
    std::vector<int> Tl, Tlp;

    bool still_simulates_irrelevant = !simulates_irrelevant[l1][lts_id];

    //For each transition from src, check if anything has changed
    lts->applyPostSrc(src, [&](const LTSTransition &tr) {
                          for (int trlabel: lts->get_labels(tr.label_group)) {
                              if (l1 == trlabel) { //Same label
                                  if (tr.target == target) {
                                      continue;
                                  }
                                  if (!still_simulates_irrelevant && sim.simulates(tr.target, tr.src)) {
                                      //There is another transition with the same label which simulates noop
                                      still_simulates_irrelevant = true;
                                  }
                                  if (!Tlbool[tr.target]) {
                                      Tl.push_back(tr.target);
                                      Tlbool[tr.target] = true;
                                  }
                              } else if (simulates(l1, trlabel, lts_id) && sim.simulates(target, tr.target)) {
                                  if (!Tlpbool[tr.target]) {
                                      Tlp.push_back(tr.target);
                                      Tlpbool[tr.target] = true;
                                  }
                              }
                          }
                          return false;
                      });
    if (!still_simulates_irrelevant) {
        return false;
    }
    for (int t: Tlp) {
        if (!Tlbool[t] &&
            find_if(std::begin(Tl), std::end(Tl), [&](int t2) {
                        return sim.simulates(t2, t);
                    }) == std::end(Tl)) {
            return false;
        }
    }

    //TODO: This should be moved somewhere else, but it is convenient to place it here.
    lts->kill_transition(src, l1, target);
    return true;
}
}
