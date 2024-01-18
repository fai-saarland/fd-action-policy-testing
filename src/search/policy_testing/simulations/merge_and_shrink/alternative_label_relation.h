#pragma once

#include "label_relation.h"
#include "../utils/utilities.h"

#include <iostream>
#include <vector>
#include "labels.h"
#include "label.h"
#include "labelled_transition_system.h"

namespace simulations {
class LabelledTransitionSystem;

/*
 * Label relation represents the preorder relations on labels that
 * occur in a set of LTS
 */
class AlternativeLabelRelation {
    Labels *labels; // TODO: currently unused private field
    int num_labels;
    int num_ltss;

    // Summary matrix for each l1, l2 indicating whether l1 dominates
    // l2 in all (-2), in none (-1) or only in i (i)
    /* std::vector<std::vector<int> > dominates_in; */
    std::vector<int>  dominated_by_noop_in;
    //std::vector<int> dominates_noop_in;

    //For each lts, matrix indicating whether l1 simulates l2, noop
    //simulates l or l simulates noop
    std::vector<int> cost_of_label;
    std::vector<std::vector<LabelGroup>> group_of_label;  //position that label l takes on lts
    std::vector<std::vector<int>> irrelevant_labels_lts;
    std::vector<std::vector<std::vector<bool>>> lrel;
    std::vector<std::vector<bool>> simulated_by_irrelevant;
    std::vector<std::vector<bool>> simulates_irrelevant;

    bool update(int i, const LabelledTransitionSystem *lts,
                const SimulationRelation &sim);


    inline bool set_not_simulates(LabelGroup lg1, LabelGroup lg2, int lts) {
        //std::cout << "Not simulates: " << l1 << " to " << l2 << " in " << lts << std::endl;
        if (lrel[lts][lg1.group][lg2.group]) {
            lrel[lts][lg1.group][lg2.group] = false;
            return true;
        }
        return false;
    }


    inline bool set_not_simulated_by_irrelevant(LabelGroup lg, int lts_id,
                                                const LabelledTransitionSystem *lts) {
        //std::cout << "Not simulated by irrelevant: " << l << " in " << lts << std::endl;

        //Returns if there were changes in dominated_by_noop_in
        int pos = lg.group;

        if (simulated_by_irrelevant[lts_id][pos]) {
            simulated_by_irrelevant[lts_id][pos] = false;

            for (int l : lts->get_labels(lg)) {
                if (dominated_by_noop_in[l] == DOMINATES_IN_ALL) {
                    dominated_by_noop_in[l] = lts_id;
                } else if (dominated_by_noop_in[l] != lts_id) {
                    dominated_by_noop_in[l] = DOMINATES_IN_NONE;
                }
            }

            return true;
        }
        return false;
    }

    inline bool set_not_simulates_irrelevant(LabelGroup lg, int lts) {
        //std::cout << "Not simulates irrelevant: " << l << " in " << lts << std::endl;

        int pos = lg.group;
        //Returns if there were changes in dominates_noop_in
        if (simulates_irrelevant[lts][pos]) {
            simulates_irrelevant[lts][pos] = false;
            return true;
        }
        return false;
    }

    [[nodiscard]] inline bool get_simulated_by_irrelevant(LabelGroup lg, int lts) const {
        return simulated_by_irrelevant[lts][lg.group];
    }

    [[nodiscard]] inline bool get_simulates_irrelevant(LabelGroup lg, int lts) const {
        return simulates_irrelevant[lts][lg.group];
    }

public:
    explicit AlternativeLabelRelation(Labels *labels);

    //Initializes label relation (only the first time, to reinitialize call reset instead)
    void init(const std::vector<LabelledTransitionSystem *> &lts,
              const DominanceRelation &sim,
              const LabelMap &labelMap);

    bool update(const std::vector<LabelledTransitionSystem *> &lts,
                const DominanceRelation &sim);


    [[nodiscard]] inline int get_num_labels() const {
        return num_labels;
    }

    [[nodiscard]] inline bool dominated_by_noop(int l, int lts) const {
        return dominated_by_noop_in[l] == DOMINATES_IN_ALL || dominated_by_noop_in[l] == lts;
    }

    [[nodiscard]] inline int get_dominated_by_noop_in(int l) const {
        return dominated_by_noop_in[l];
    }

    //Returns true if l1 simulates l2 in lts
    [[nodiscard]] inline bool simulates(LabelGroup lg1, LabelGroup lg2, int lts) const {
        int pos1 = lg1.group;
        int pos2 = lg2.group;
        if (pos1 >= 0) {
            if (pos2 >= 0) {
                return lrel[lts][pos1][pos2];
            } else {
                return simulates_irrelevant[lts][pos1];
            }
        } else {
            if (pos2 != -1) {
                return simulated_by_irrelevant[lts][pos2];
            } else {
                return true; //Both are irrelevant
            }
        }
    }

    //Returns true if l1 simulates l2 in lts
    [[nodiscard]] inline bool get_simulates(int l1, int l2, int lts) const {
        return simulates(group_of_label[lts][l1], group_of_label[lts][l2], lts);
    }

    //Returns true if l1 simulates l2 in lts
    [[nodiscard]] bool dominates(int l1, int l2, int lts) const {
        if (cost_of_label[l2] < cost_of_label[l1])
            return false;
        for (int lts_id = 0; lts_id < num_ltss; ++lts_id) {
            if (lts_id != lts && !get_simulates(l1, l2, lts_id)) {
                assert(num_ltss > 1);
                return false;
            }
        }
        return true;
    }


    [[nodiscard]] int get_label_cost(int label) const {
        return cost_of_label[label];
    }

    void dump(const LabelledTransitionSystem *lts, int lts_id) const;


    [[nodiscard]] bool propagate_transition_pruning(int,
                                                    const std::vector<LabelledTransitionSystem *> &,
                                                    const DominanceRelation &,
                                                    int, int, int) const {
        std::cout << "propagate_transition_pruning not implemented." << std::endl;
        exit(EXIT_CRITICAL_ERROR);
        return false;
    }


    void kill_label(int) {
        std::cout << "kill_label not implemented." << std::endl;
        exit(EXIT_CRITICAL_ERROR);
    }

    [[nodiscard]] std::vector<int> get_labels_dominated_in_all() const;

    EquivalenceRelation *get_equivalent_labels_relation(const LabelMap &, std::set<int> &) const {
        std::cout << "get_equivalent_labels_relation." << std::endl;
        exit(EXIT_CRITICAL_ERROR);
    }
};
}
