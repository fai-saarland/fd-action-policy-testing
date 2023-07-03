#pragma once

#include "../merge_and_shrink/label_relation.h"

#include <iostream>
#include <vector>
#include <limits>
#include "../merge_and_shrink/abstraction.h"
#include "../merge_and_shrink/labels.h"
#include "../merge_and_shrink/label.h"
#include "../merge_and_shrink/labelled_transition_system.h"
#include "int_epsilon.h"

namespace simulations {
class LabelledTransitionSystem;

template<typename T>
class NumericDominanceRelation;

template<typename T>
class NumericSimulationRelation;

/*
 * Label relation represents the preorder relations on labels that
 * occur in a set of LTS
 */
template<typename T>
class NumericLabelRelation {
    Labels *labels;
    int num_labels;
    int num_ltss;

    const int num_labels_to_use_dominates_in;

    // Summary matrix for each l1, l2 indicating whether l1 dominates
    // l2 in all (DOMINATES_IN_ALL), in none (DOMINATES_IN_NONE) or in all but i (i)
    std::vector<std::vector<int>> dominates_in;
    std::vector<int> dominates_noop_in, dominated_by_noop_in;

    //For each lts, matrix indicating whether l1 simulates l2, noop
    //simulates l or l simulates noop
    std::vector<T> cost_of_label;
    std::vector<std::vector<LabelGroup>> group_of_label;     //position that label l takes on lts
    std::vector<std::vector<int>> irrelevant_labels_lts;
    std::vector<std::vector<std::vector<T>>> lqrel;
    std::vector<std::vector<T>> simulated_by_irrelevant;
    std::vector<std::vector<T>> simulates_irrelevant;

    /* std::shared_ptr<TauLabelManager<T>> tau_labels; */

    bool update(int i, const LabelledTransitionSystem *lts,
                const NumericSimulationRelation <T> &sim);

    inline T get_lqrel(LabelGroup lgroup1, LabelGroup lgroup2, int lts) const {
        int pos1 = lgroup1.group;
        int pos2 = lgroup2.group;

        //int pos1 = position_of_label[lts][l1];
        //int pos2 = position_of_label[lts][l2];
        if (pos1 >= 0) {
            if (pos2 >= 0) {
                return lqrel[lts][pos1][pos2];
            } else {
                return simulates_irrelevant[lts][pos1];
            }
        } else {
            if (pos2 != -1) {
                return simulated_by_irrelevant[lts][pos2];
            } else {
                return 0;     //Both are irrelevant
            }
        }
    }

    inline T get_lqrel(int l1, int l2, int lts) const {
        return get_lqrel(group_of_label[lts][l1], group_of_label[lts][l2], lts);
    }

    inline bool set_lqrel(LabelGroup lgroup1, LabelGroup lgroup2, int lts_id,
                          const LabelledTransitionSystem *lts, T value) {
        assert(value != MINUS_INFINITY + 1);
        /* assert(dominates_in.empty() ||
           dominates_in[l1][l2] == DOMINATES_IN_ALL || dominates_in[l1][l2] != lts_id); */
        /* int pos1 = position_of_label[lts_id][l1]; */
        /* int pos2 = position_of_label[lts_id][l2]; */

        assert(lgroup1.group >= 0 && lgroup2.group >= 0);
        const unsigned int pos1 = lgroup1.group;
        const unsigned int pos2 = lgroup2.group;

        assert(lts_id >= 0 && lts_id < lqrel.size());
        assert(pos1 < lqrel[lts_id].size());
        assert(pos2 < lqrel[lts_id][pos1].size());

        assert(value <= lqrel[lts_id][pos1][pos2]);
        if (value < lqrel[lts_id][pos1][pos2]) {
            lqrel[lts_id][pos1][pos2] = value;
            if (value == MINUS_INFINITY && !dominates_in.empty()) {
                for (int l1: lts->get_labels(lgroup1)) {
                    for (int l2: lts->get_labels(lgroup2)) {
                        if (dominates_in[l1][l2] == DOMINATES_IN_ALL) {
                            dominates_in[l1][l2] = lts_id;
                        } else if (dominates_in[l1][l2] != lts_id) {
                            dominates_in[l1][l2] = DOMINATES_IN_NONE;
                        }
                    }
                }
            }
            return true;
        }
        return false;
    }

    inline T get_simulated_by_irrelevant(int l, int lts) const {
        if (!group_of_label[lts][l].dead()) {
            return simulated_by_irrelevant[lts][group_of_label[lts][l].group];
        } else {
            return 0;
        }
    }

    inline T get_simulated_by_irrelevant(LabelGroup lgroup, int lts) const {
        if (!lgroup.dead()) {
            return simulated_by_irrelevant[lts][lgroup.group];
        } else {
            return 0;
        }
    }

    inline bool set_simulated_by_irrelevant(LabelGroup lgroup, int lts_id,
                                            const LabelledTransitionSystem *lts,
                                            T value) {
        //Returns if there were changes in dominated_by_noop_in
        //int pos = position_of_label[lts_id][l];
        int pos = lgroup.group;
        assert(pos >= 0);

        assert(value <= simulated_by_irrelevant[lts_id][pos]);
        assert(value != MINUS_INFINITY + 1);
        if (value < simulated_by_irrelevant[lts_id][pos]) {
            simulated_by_irrelevant[lts_id][pos] = value;
            if (value == MINUS_INFINITY) {
                for (int l: lts->get_labels(lgroup)) {
                    if (dominated_by_noop_in[l] == DOMINATES_IN_ALL) {
                        dominated_by_noop_in[l] = lts_id;
                    } else if (dominated_by_noop_in[l] != lts_id) {
                        dominated_by_noop_in[l] = DOMINATES_IN_NONE;
                    }
                    if (!dominates_in.empty()) {
                        for (int l1: irrelevant_labels_lts[lts_id]) {
                            if (dominates_in[l1][l] == DOMINATES_IN_ALL) {
                                dominates_in[l1][l] = lts_id;
                            } else if (dominates_in[l1][l] != lts_id) {
                                dominates_in[l1][l] = DOMINATES_IN_NONE;
                            }
                        }
                    }
                }
            }
            return true;
        }
        return false;
    }


    inline T get_simulates_irrelevant(LabelGroup lgroup, int lts) const {
        if (!lgroup.dead()) {
            return simulates_irrelevant[lts][lgroup.group];
        } else {
            return 0;
        }
    }


    inline bool set_simulates_irrelevant(LabelGroup lgroup, int lts_id,
                                         const LabelledTransitionSystem *lts,
                                         T value) {
        assert(value != MINUS_INFINITY + 1);

        int pos = lgroup.group;
        assert(pos >= 0);

        //Returns if there were changes in dominates_noop_in
        assert(value <= simulates_irrelevant[lts_id][pos]);
        if (value < simulates_irrelevant[lts_id][pos]) {
            simulates_irrelevant[lts_id][pos] = value;

            if (value == MINUS_INFINITY) {
                for (int l: lts->get_labels(lgroup)) {
                    if (dominates_noop_in[l] == DOMINATES_IN_ALL) {
                        dominates_noop_in[l] = lts_id;
                    } else if (dominates_noop_in[l] != lts_id) {
                        dominates_noop_in[l] = DOMINATES_IN_NONE;
                    }
                    if (!dominates_in.empty()) {
                        for (int l2: irrelevant_labels_lts[lts_id]) {
                            if (dominates_in[l][l2] == DOMINATES_IN_ALL) {
                                dominates_in[l][l2] = lts_id;
                            } else if (dominates_in[l][l2] != lts_id) {
                                dominates_in[l][l2] = DOMINATES_IN_NONE;
                            }
                        }
                    }
                }
            }
            return true;
        }
        return false;
    }

    /* int mix_numbers(const std::vector<int> & values,  */
    /*              const std::vector<int> & values_irrelevant_labels,  */
    /*              int lts) const;  */

public:
    NumericLabelRelation(Labels *labels,
                         int num_labels_to_use_dominates_in /* , std::shared_ptr<TauLabelManager<T>> tau_labels_mgr */);

    //Initializes label relation (only the first time, to reinitialize call reset instead)

    template<typename NDR>
    void init(const std::vector<LabelledTransitionSystem *> &lts,
              const NDR &sim, const LabelMap &labelMap) {
        num_labels = labelMap.get_num_labels();
        num_ltss = lts.size();

        std::cout << "Init label dominance: " << num_labels
                  << " labels " << lts.size() << " systems.\n";


        std::vector<T>().swap(cost_of_label);
        std::vector<std::vector<LabelGroup>>().swap(group_of_label);
        std::vector<std::vector<std::vector<T>>>().swap(lqrel);
        std::vector<std::vector<T>>().swap(simulates_irrelevant);
        std::vector<std::vector<T>>().swap(simulated_by_irrelevant);

        irrelevant_labels_lts.resize(lts.size());
        group_of_label.resize(lts.size());
        simulates_irrelevant.resize(lts.size());
        simulated_by_irrelevant.resize(lts.size());
        lqrel.resize(lts.size());

        cost_of_label.resize(num_labels);
        for (int l = 0; l < num_labels; l++) {
            cost_of_label[l] = labelMap.get_cost(l);
        }

        for (int i = 0; i < num_ltss; ++i) {
            group_of_label[i] = lts[i]->get_group_of_label();
            irrelevant_labels_lts[i] = lts[i]->get_irrelevant_labels();

            int num_label_groups = lts[i]->get_num_label_groups();

            /* std::cout << "Relevant label groups: " << num_label_groups << "\n"; */

            simulates_irrelevant[i].resize(num_label_groups, std::numeric_limits<int>::max());
            simulated_by_irrelevant[i].resize(num_label_groups, std::numeric_limits<int>::max());
            lqrel[i].resize(num_label_groups);

            for (int j = 0; j < num_label_groups; j++) {
                lqrel[i][j].resize(num_label_groups, std::numeric_limits<int>::max());
                lqrel[i][j][j] = 0;
            }
        }

        std::cout << "Dominating.\n";
        std::vector<std::vector<int>>().swap(dominates_in);
        std::vector<int>().swap(dominated_by_noop_in);
        std::vector<int>().swap(dominates_noop_in);
        dominated_by_noop_in.resize(num_labels, DOMINATES_IN_ALL);
        dominates_noop_in.resize(num_labels, DOMINATES_IN_ALL);

        if (num_labels < num_labels_to_use_dominates_in) {
            dominates_in.resize(num_labels);
            for (auto &domination_vector : dominates_in) {
                domination_vector.resize(num_labels, DOMINATES_IN_ALL);
            }
        }
        std::cout << "Update label dominance: " << num_labels
                  << " labels " << lts.size() << " systems.\n";

        for (int i = 0; i < num_ltss; ++i) {
            update(i, lts[i], sim[i]);
        }
    }


    template<typename NDR>
    bool update(const std::vector<LabelledTransitionSystem *> &lts, const NDR &sim) {
        bool changes = false;

        for (unsigned int i = 0; i < lts.size(); ++i) {
            changes |= update(i, lts[i], sim[i]);
        }

        return changes;
    }

    [[nodiscard]] inline int get_num_labels() const {
        return num_labels;
    }

    /// Returns true if l is simulated by noop in all j \neq lts
    [[nodiscard]] inline bool simulated_by_noop_in_all_other(int l, int lts) const {
        return dominated_by_noop_in[l] == DOMINATES_IN_ALL || dominated_by_noop_in[l] == lts;
    }

    /// Returns true if l simulates noop in all j \neq lts
    [[nodiscard]] inline bool simulates_noop_in_all_other(int l, int lts) const {
        return dominates_noop_in[l] == DOMINATES_IN_ALL || dominates_noop_in[l] == lts;
    }

    [[nodiscard]] inline bool dominates_noop_in_all(int l) const {
        return dominates_noop_in[l] == DOMINATES_IN_ALL;
    }

    [[nodiscard]] inline int get_dominates_noop_in(int l) const {
        return dominates_noop_in[l];
    }

    /** @warning ignores the DOMINATES_IN_ALL case */
    [[nodiscard]] inline bool dominates_noop_in_all_but_one(int l) const {
        return dominates_noop_in[l] >= 0;
    }


    /// Returns true if l simulates l2 in all j \neq lts
    [[nodiscard]] inline bool simulates_in_all_other(int l1, int l2, int lts) const {
        if (dominates_in.empty()) {
            for (int lts_id = 0; lts_id < num_ltss; ++lts_id) {
                if (lts_id != lts && get_lqrel(l1, l2, lts_id) == MINUS_INFINITY) {
                    assert(num_ltss > 1);
                    return false;
                }
            }
            return true;
        }

        assert(num_ltss > 1 || dominates_in[l1][l2] == DOMINATES_IN_ALL || (dominates_in[l1][l2] == lts));

#ifndef NDEBUG
        if (dominates_in[l1][l2] == DOMINATES_IN_ALL || (dominates_in[l1][l2] == lts)) {
            for (int lts_id = 0; lts_id < num_ltss; ++lts_id) {
                if (!(lts_id == lts || get_lqrel(l1, l2, lts_id) != MINUS_INFINITY)) {
                    std::cout << this << "l1: " << l1 << " l2: " << l2 << " lts: " << lts_id << " group1: "
                              << group_of_label[lts_id][l1].group << " group2: " << group_of_label[lts_id][l2].group
                              << std::endl;
                }
                assert(lts_id == lts || get_lqrel(l1, l2, lts_id) != MINUS_INFINITY);
            }
        }
#endif
        return dominates_in[l1][l2] == DOMINATES_IN_ALL || (dominates_in[l1][l2] == lts);
    }

    //Returns true if l1 simulates l2 in lts
    [[nodiscard]] inline bool may_simulate(LabelGroup lgroup1, LabelGroup lgroup2, int lts) const {
        return get_lqrel(lgroup1, lgroup2, lts) != MINUS_INFINITY;
    }

    /* //Returns true if l1 simulates l2 in lts */
    /* inline bool may_simulate (int l1, int l2, int lts) const{ */
    /*  if(dominates_in.empty()) { */
    /*      return get_lqrel(l1, l2, lts) != MINUS_INFINITY; */
    /*  } */
    /*     return dominates_in[l1][l2] !=  DOMINATES_IN_NONE && */
    /*      (dominates_in[l1][l2] == DOMINATES_IN_ALL || */
    /*       dominates_in[l1][l2] != lts); */
    /* } */


    T q_dominates_value(int l1, int l2, int lts) const {
        if (simulates_in_all_other(l1, l2, lts)) {
            T total_sum = 0;

            for (int lts_id = 0; lts_id < num_ltss; ++lts_id) {
                if (lts_id != lts) {
                    assert(get_lqrel(l1, l2, lts_id) != MINUS_INFINITY);
                    total_sum += get_lqrel(l1, l2, lts_id);
                }
            }

            assert(num_ltss > 0 || total_sum == T(0));

            return total_sum;
        } else {
            assert(false);
            return MINUS_INFINITY;
        }
    }

    T q_dominates_noop(int l, int lts) const {
        if (simulates_noop_in_all_other(l, lts)) {
            T total_sum = 0;

            for (int lts_id = 0; lts_id < num_ltss; ++lts_id) {
                if (lts_id != lts) {
                    assert(get_simulates_irrelevant(group_of_label[lts_id][l], lts_id) != MINUS_INFINITY);
                    total_sum += get_simulates_irrelevant(group_of_label[lts_id][l], lts_id);
                }
            }
            return total_sum;
        } else {
            assert(false);
            return MINUS_INFINITY;
        }
    }

    T q_dominated_by_noop(int l, int lts) const {
        if (simulated_by_noop_in_all_other(l, lts)) {
            T total_sum = 0;

            for (int lts_id = 0; lts_id < num_ltss; ++lts_id) {
                if (lts_id != lts) {
                    assert(get_simulated_by_irrelevant(group_of_label[lts_id][l], lts_id) != MINUS_INFINITY);
                    total_sum += get_simulated_by_irrelevant(group_of_label[lts_id][l], lts_id);
                }
            }
            return total_sum;
        } else {
            assert(false);
            return MINUS_INFINITY;
        }
    }

    [[nodiscard]] T get_label_cost(int label) const {
        return cost_of_label[label];
    }

    void dump(const LabelledTransitionSystem *lts, int lts_id) const;
};
}
