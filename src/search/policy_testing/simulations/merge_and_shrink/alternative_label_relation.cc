#include "alternative_label_relation.h"

#include "labels.h"
#include "labelled_transition_system.h"
#include "simulation_relation.h"
#include "dominance_relation.h"

#include "../utils/debug.h"

namespace simulations {
AlternativeLabelRelation::AlternativeLabelRelation(Labels *_labels) :
    labels(_labels), num_labels(_labels->get_size()) {
}


void AlternativeLabelRelation::init(const std::vector<LabelledTransitionSystem *> &lts,
                                    const DominanceRelation &sim,
                                    const LabelMap &labelMap) {
    num_labels = labelMap.get_num_labels();
    num_ltss = lts.size();

    std::cout << "Init label dominance: " << num_labels
              << " labels " << lts.size() << " systems." << std::endl;

    std::vector<int>().swap(cost_of_label);
    std::vector<std::vector<LabelGroup>>().swap(group_of_label);
    std::vector<std::vector<std::vector<bool>>>().swap(lrel);
    std::vector<std::vector<bool>>().swap(simulates_irrelevant);
    std::vector<std::vector<bool>>().swap(simulated_by_irrelevant);

    irrelevant_labels_lts.resize(lts.size());
    group_of_label.resize(lts.size());
    simulates_irrelevant.resize(lts.size());
    simulated_by_irrelevant.resize(lts.size());
    lrel.resize(lts.size());

    cost_of_label.resize(num_labels);
    for (int l = 0; l < num_labels; l++) {
        cost_of_label[l] = labelMap.get_cost(l);
    }

    for (int i = 0; i < num_ltss; ++i) {
        group_of_label[i] = lts[i]->get_group_of_label();
        irrelevant_labels_lts[i] = lts[i]->get_irrelevant_labels();

        int num_label_groups = lts[i]->get_num_label_groups();

        simulates_irrelevant[i].resize(num_label_groups, true);
        simulated_by_irrelevant[i].resize(num_label_groups, true);
        lrel[i].resize(num_label_groups);

        for (int j = 0; j < num_label_groups; j++) {
            lrel[i][j].resize(num_label_groups, true);
        }
    }

    std::cout << "Dominating." << std::endl;
    std::vector<int>().swap(dominated_by_noop_in);
    dominated_by_noop_in.resize(num_labels, DOMINATES_IN_ALL);

    std::cout << "Update label dominance: " << num_labels
              << " labels " << lts.size() << " systems." << std::endl;

    for (int i = 0; i < num_ltss; ++i) {
        update(i, lts[i], sim[i]);
    }
}


bool AlternativeLabelRelation::update(const std::vector<LabelledTransitionSystem *> &lts,
                                      const DominanceRelation &sim) {
    bool changes = false;

    for (int i = 0; i < lts.size(); ++i) {
        changes |= update(i, lts[i], sim[i]);
    }

    return changes;
}


bool AlternativeLabelRelation::update(int lts_i, const LabelledTransitionSystem *lts,
                                      const SimulationRelation &sim) {
    bool changes = false;

    for (LabelGroup lg2(0); lg2.group < lts->get_num_label_groups(); ++lg2) {
        for (LabelGroup lg1(0); lg1.group < lts->get_num_label_groups(); ++lg1) {
            if (lg1 != lg2 && simulates(lg1, lg2, lts_i)) {
                //Check if it really simulates
                //For each transition s--l2-->t, and every label l1 that dominates
                //l2, exist s--l1-->t', t <= t'?
                for (const auto &tr: lts->get_transitions_label_group(lg2)) {
                    bool found = false;
                    //TODO: for(auto tr2 : lts->get_transitions_for_label_src(l1, tr.src)){
                    for (const auto &tr2: lts->get_transitions_label_group(lg1)) {
                        if (tr2.src == tr.src && sim.simulates(tr2.target, tr.target)) {
                            found = true;
                            break;     //Stop checking this transition
                        }
                    }
                    if (!found) {
                        set_not_simulates(lg1, lg2, lts_i);
                        changes = true;
                        break;     //Stop checking trs of l1
                    }
                }
            }
        }

        //Is l2 simulated by irrelevant_labels in lts?
        if (get_simulated_by_irrelevant(lg2, lts_i)) {
            for (auto tr: lts->get_transitions_label_group(lg2)) {
                if (!sim.simulates(tr.src, tr.target)) {
                    changes |= set_not_simulated_by_irrelevant(lg2, lts_i, lts);
                    break;
                }
            }
        }

        //Does l2 simulates irrelevant_labels in lts?
        if (get_simulates_irrelevant(lg2, lts_i)) {
            for (int s = 0; s < lts->size(); s++) {
                bool found = false;
                for (const auto &tr: lts->get_transitions_label_group(lg2)) {
                    if (tr.src == s && sim.simulates(tr.target, tr.src)) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    changes |= set_not_simulates_irrelevant(lg2, lts_i);
                    break;
                }
            }
        }
    }

    return changes;
}


void AlternativeLabelRelation::dump(const LabelledTransitionSystem *, int) const {
}

std::vector<int> AlternativeLabelRelation::get_labels_dominated_in_all() const {
    std::cout << "AlternativeLabelRelation::get_labels_dominated_in_all not implemented." << std::endl;
    utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
}
}
