#include "labels.h"
#include "label.h"
#include "label_reducer.h"
#include "dominance_relation.h"

#include "../utils/debug.h"
#include "../utils/equivalence_relation.h"

#include <algorithm>
#include <cassert>
#include <set>


namespace simulations {
Labels::Labels(bool unit_cost_, const Options &options, OperatorCost cost_type)
    : unit_cost(unit_cost_) {
    label_reducer = new LabelReducer(options);
    const int num_operators = global_simulation_task()->get_num_operators();
    if (num_operators) {
        labels.reserve(num_operators * 2 - 1);
    }
    for (int i = 0; i < num_operators; ++i) {
        DEBUG_MSG(std::cout << "OPERATOR " << i << ": "
                            << global_simulation_task()->get_operator_name(i, false) << std::endl;
                  );
        labels.push_back(new OperatorLabel(i, get_adjusted_action_cost(get_op_proxy(i), cost_type, is_unit_cost()),
                                           get_prevails(i), get_preposts(i)));
    }
}

Labels::~Labels() {
    delete label_reducer;
}

void Labels::reduce(std::pair<int, int> next_merge,
                    const std::vector<Abstraction *> &all_abstractions) {
    label_reducer->reduce_labels(next_merge, all_abstractions, labels);
}

void Labels::reduce(const LabelMap &labelMap, const DominanceRelation &dominance_relation,
                    std::set<int> &dangerous_LTSs) {
    EquivalenceRelation *equiv_rel = dominance_relation.get_equivalent_labels_relation(labelMap,
                                                                                       dangerous_LTSs);
    simulations::LabelReducer::reduce_exactly(equiv_rel, labels);
    delete equiv_rel;
}

void Labels::reduce_to_cost() {
    label_reducer->reduce_labels_to_cost(labels);
}

const Label *Labels::get_label_by_index(int index) const {
    assert(index >= 0 && index < labels.size());
    return labels[index];
}

bool Labels::is_label_reduced(int label_no) const {
    return get_label_by_index(label_no)->is_reduced();
}

int Labels::get_label_cost(int label_no) const {
    return get_label_by_index(label_no)->get_cost();
}

void Labels::dump() const {
    std::cout << "no of labels: " << labels.size() << std::endl;
    for (auto label : labels) {
        label->dump();
    }
}

void Labels::dump_options() const {
    label_reducer->dump_options();
}

void Labels::reset_relevant_for(const std::vector<Abstraction *> &abstractions) {
    for (auto &label : labels) {
        label->reset_relevant_for(abstractions);
    }
}

void Labels::set_irrelevant_for(int label_no, Abstraction *abstraction) {
    labels[label_no]->set_irrelevant_for(abstraction);
}

void Labels::set_irrelevant_for_all_labels(Abstraction *abstraction) {
    for (auto &label : labels) {
        label->set_irrelevant_for(abstraction);
    }
}

void Labels::set_relevant_for(int label_no, Abstraction *abstraction) {
    labels[label_no]->set_relevant_for(abstraction);
}

const std::set<Abstraction *> &Labels::get_relevant_for(int label_no) const {
    return labels[label_no]->get_relevant_for();
}

void Labels::prune_irrelevant_labels() {
    std::set<int> ops;
    for (auto label: labels) {
        if (label->is_irrelevant()) {
            label->get_operators(ops);
        }
    }
    std::cout << ops.size() << " irrelevant operators." << std::endl;
    for (int op: ops) {
        std::cout << "Irrelevant operator: " << global_simulation_task()->get_operator_name(op, false) << std::endl;
    }
}


bool Labels::applies_perfect_label_reduction() const {
    return label_reducer->applies_perfect_label_reduction();
}
}
