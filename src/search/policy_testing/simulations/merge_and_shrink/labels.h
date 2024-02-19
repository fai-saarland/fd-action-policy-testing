#pragma once

#include "../../../operator_cost.h"

#include <vector>
#include <set>
#include <list>

namespace plugins {
class Options;
}

namespace simulations {
class Abstraction;
class Label;
class LabelMap;
class DominanceRelation;
class LabelReducer;

/*
 The Labels class is basically a container class for the set of all
 labels used by merge-and-shrink abstractions.
 */
class Labels {
    const bool unit_cost;
    const LabelReducer *label_reducer;

    std::vector<Label *> labels;

public:
    Labels(bool unit_cost, const plugins::Options &options, OperatorCost cost_type);
    ~Labels();
    void reduce(std::pair<int, int> next_merge,
                const std::vector<Abstraction *> &all_abstractions);
    void reduce_to_cost();
    void reduce(const LabelMap &labelMap, const DominanceRelation &dominance_relation,
                std::set<int> &dangerous_LTSs);
    // TODO: consider removing get_label_by_index and forwarding all required
    // methods of Label and giving access to them by label number.
    [[nodiscard]] const Label *get_label_by_index(int index) const;
    [[nodiscard]] bool is_label_reduced(int label_no) const;
    void dump() const;
    void dump_options() const;

    [[nodiscard]] int get_size() const {
        return labels.size();
    }
    [[nodiscard]] bool is_unit_cost() const {
        return unit_cost;
    }

    void set_relevant_for(int label_no, Abstraction *abstraction);
    void set_irrelevant_for(int label_no, Abstraction *abstraction);
    void set_irrelevant_for_all_labels(Abstraction *abstraction);

    [[nodiscard]] const std::set<Abstraction *> &get_relevant_for(int label_no) const;

    // void prune_irrelevant_labels();

    void reset_relevant_for(const std::vector<Abstraction *> &abstractions);

    // [[nodiscard]] bool applies_perfect_label_reduction() const;
    [[nodiscard]] int  get_label_cost(int label_no) const;
};


class LabelMap {
    const Labels *labels;
    // mapping from labels to labels for LTSs (hack to get rid of not useful labels)
    int num_valid_labels;
    std::vector<int> label_id;
    std::vector<int> old_label_id;
public:
    explicit LabelMap(Labels *_labels) : labels(_labels) {
        //TODO: Ensure that all dead labels are removed
        num_valid_labels = 0;
        label_id.reserve(labels->get_size());
        old_label_id.reserve(labels->get_size());
        for (int i = 0; i < labels->get_size(); i++) {
            if (labels->is_label_reduced(i)) {
                label_id.push_back(-1);
            } else {
                old_label_id.push_back(i);
                label_id.push_back(num_valid_labels++);
            }
        }
    }

    [[nodiscard]] int get_id(int i) const {
        return label_id[i];
    }
    [[nodiscard]] int get_old_id(int i) const {
        return old_label_id[i];
    }

    [[nodiscard]] int get_num_labels() const {
        return num_valid_labels;
    }
    [[nodiscard]] int get_cost(int l) const {
        return labels->get_label_cost(get_old_id(l));
    }
};
}
