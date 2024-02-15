#include "label_reducer.h"

#include "abstraction.h"
#include "label.h"
#include "../utils/equivalence_relation.h"
#include "../../../plugins/plugin.h"

#include <cassert>
#include <unordered_map>
#include <map>
#include <iostream>
#include <limits>
#include <random>

namespace simulations {
LabelReducer::LabelReducer(const plugins::Options &options)
    : label_reduction_method(options.get<LabelReductionMethod>("label_reduction_method")),
      label_reduction_system_order(options.get<LabelReductionSystemOrder>("label_reduction_system_order")),
      max_time(options.get<int>("label_reduction_max_time")) {
    std::size_t max_no_systems = global_simulation_task()->get_num_variables() * 2 - 1;
    system_order.reserve(max_no_systems);
    if (label_reduction_system_order == LabelReductionSystemOrder::REGULAR
        || label_reduction_system_order == LabelReductionSystemOrder::RANDOM) {
        for (std::size_t i = 0; i < max_no_systems; ++i)
            system_order.push_back(i);
        if (label_reduction_system_order == LabelReductionSystemOrder::RANDOM) {
            std::shuffle(system_order.begin(), system_order.end(), std::mt19937(std::random_device()()));
        }
    } else {
        assert(label_reduction_system_order == LabelReductionSystemOrder::REVERSE);
        for (std::size_t i = 0; i < max_no_systems; ++i)
            system_order.push_back(max_no_systems - 1 - i);
    }
}

void LabelReducer::reduce_labels(std::pair<int, int> next_merge,
                                 const std::vector<Abstraction *> &all_abstractions,
                                 std::vector<Label *> &labels) const {
    if (label_reduction_method == LabelReductionMethod::NONE) {
        return;
    }

    if (label_reduction_method == LabelReductionMethod::OLD) {
        // We need to normalize all abstraction to incorporate possible previous
        // label reductions, because normalize cannot deal with several label
        // reductions at once.
        for (auto abstraction : all_abstractions) {
            if (abstraction) {
                abstraction->normalize();
            }
        }
        assert(all_abstractions[next_merge.first]->get_varset().size() >=
               all_abstractions[next_merge.second]->get_varset().size());
        reduce_old(all_abstractions[next_merge.first]->get_varset(), labels);
        return;
    }

    if (label_reduction_method == LabelReductionMethod::TWO_ABSTRACTIONS) {
        /* Note:
           We compute the combinable relation for labels for the two abstractions
           in the order given by the merge strategy. We conducted experiments
           testing the impact of always starting with the larger abstraction
           (in terms of variables) or with the smaller abstraction and found
           no significant differences.
         */
        assert(all_abstractions[next_merge.first]);
        assert(all_abstractions[next_merge.second]);

        std::vector<EquivalenceRelation *> local_equivalence_relations(all_abstractions.size(), nullptr);

        EquivalenceRelation *relation = compute_outside_equivalence(
            next_merge.first, all_abstractions,
            labels, local_equivalence_relations);
        reduce_exactly(relation, labels);
        delete relation;

        relation = compute_outside_equivalence(
            next_merge.second, all_abstractions,
            labels, local_equivalence_relations);
        reduce_exactly(relation, labels);
        delete relation;

        for (auto &local_equivalence_relation : local_equivalence_relations)
            delete local_equivalence_relation;
        return;
    }

    // Make sure that we start with an index not out of range for
    // all_abstractions
    std::size_t system_order_index = 0;
    assert(!system_order.empty());
    while (system_order[system_order_index] >= all_abstractions.size()) {
        ++system_order_index;
        assert(system_order_index < system_order.size());
    }

    int max_iterations;
    if (label_reduction_method == LabelReductionMethod::ALL_ABSTRACTIONS) {
        max_iterations = all_abstractions.size();
    } else if (label_reduction_method == LabelReductionMethod::ALL_ABSTRACTIONS_WITH_FIXPOINT) {
        max_iterations = std::numeric_limits<int>::max();
    } else {
        abort();
    }

    int num_unsuccessful_iterations = 0;
    std::vector<EquivalenceRelation *> local_equivalence_relations(
        all_abstractions.size(), nullptr);

    utils::Timer t;
    for (int i = 0; i < max_iterations && t() < max_time; ++i) {
        std::size_t abs_index = system_order[system_order_index];
        Abstraction *current_abstraction = all_abstractions[abs_index];

        bool have_reduced = false;
        if (current_abstraction != nullptr) {
            EquivalenceRelation *relation = compute_outside_equivalence(
                abs_index, all_abstractions,
                labels, local_equivalence_relations);
            have_reduced = reduce_exactly(relation, labels);
            delete relation;
        }

        if (have_reduced) {
            num_unsuccessful_iterations = 0;
        } else {
            ++num_unsuccessful_iterations;
        }

        if (num_unsuccessful_iterations == all_abstractions.size() - 1)
            break;

        ++system_order_index;
        if (system_order_index == system_order.size()) {
            system_order_index = 0;
        }
        while (system_order[system_order_index] >= all_abstractions.size()) {
            ++system_order_index;
            if (system_order_index == system_order.size()) {
                system_order_index = 0;
            }
        }
    }

    for (auto &local_equivalence_relation : local_equivalence_relations)
        delete local_equivalence_relation;
}


void LabelReducer::reduce_labels_to_cost(std::vector<Label *> &labels) const {
    if (label_reduction_method == LabelReductionMethod::NONE) {
        return;
    }
    int num_labels = labels.size();

    std::vector<std::pair<int, int>> annotated_labels;
    std::map<int, int> costID;
    int numids = 0;
    annotated_labels.reserve(num_labels);
    for (int label_no = 0; label_no < num_labels; ++label_no) {
        const Label *label = labels[label_no];
        assert(label->get_id() == label_no);
        if (!label->is_reduced()) {
            int labelcost = label->get_cost();
            int id;
            if (costID.count(labelcost)) {
                id = costID[labelcost];
            } else {
                id = numids++;
                costID[labelcost] = id;
            }
            // only consider non-reduced labels
            annotated_labels.emplace_back(id, label_no);
        }
    }

    EquivalenceRelation *relation =
        EquivalenceRelation::from_annotated_elements<int>(num_labels, annotated_labels);
    reduce_exactly(relation, labels);
    delete relation;
}

typedef std::pair<int, int> Assignment;

struct LabelSignature {
    std::vector<int> data;

    LabelSignature(const std::vector<Assignment> &preconditions,
                   const std::vector<Assignment> &effects, int cost) {
        // We require that preconditions and effects are sorted by
        // variable -- some sort of canonical representation is needed
        // to guarantee that we can properly test for uniqueness.
        for (std::size_t i = 0; i < preconditions.size(); ++i) {
            if (i != 0)
                assert(preconditions[i].first > preconditions[i - 1].first);
            data.push_back(preconditions[i].first);
            data.push_back(preconditions[i].second);
        }
        data.push_back(-1);     // marker
        for (std::size_t i = 0; i < effects.size(); ++i) {
            if (i != 0)
                assert(effects[i].first > effects[i - 1].first);
            data.push_back(effects[i].first);
            data.push_back(effects[i].second);
        }
        data.push_back(-1);     // marker
        data.push_back(cost);
    }

    bool operator==(const LabelSignature &other) const {
        return data == other.data;
    }

    [[nodiscard]] std::size_t hash() const {
        return hash_number_sequence(data, data.size());
    }
};
}

namespace std {
template<>
struct hash<simulations::LabelSignature> {
    std::size_t operator()(const simulations::LabelSignature &sig) const {
        return sig.hash();
    }
};
}

namespace simulations {
LabelSignature LabelReducer::build_label_signature(
    const Label &label,
    const std::vector<bool> &var_is_used) {
    std::vector<Assignment> preconditions;
    std::vector<Assignment> effects;

    const std::vector<Prevail> &prevs = label.get_prevail();
    for (const auto &prev : prevs) {
        int var = prev.var;
        if (var_is_used[var]) {
            int val = prev.prev;
            preconditions.emplace_back(var, val);
        }
    }
    const std::vector<PrePost> &pre_posts = label.get_pre_post();
    for (const auto &pre_post : pre_posts) {
        int var = pre_post.var;
        if (var_is_used[var]) {
            int pre = pre_post.pre;
            if (pre != -1)
                preconditions.emplace_back(var, pre);
            int post = pre_post.post;
            effects.emplace_back(var, post);
        }
    }
    std::sort(preconditions.begin(), preconditions.end());
    std::sort(effects.begin(), effects.end());

    return {preconditions, effects, label.get_cost()};
}

bool LabelReducer::reduce_old(const std::vector<int> &abs_vars, std::vector<Label *> &labels) {
    int num_labels = 0;
    int num_labels_after_reduction = 0;

    std::vector<bool> var_is_used(global_simulation_task()->get_num_variables(), true);
    for (int abs_var : abs_vars)
        var_is_used[abs_var] = false;

    std::unordered_map<LabelSignature, std::vector<Label *>> reduced_label_map;
    // TODO: consider combining reduced_label_signature and is_label_reduced
    // into a set or hash-set (is_label_reduced only serves to make sure
    // that every label signature is pushed at most once into reduced_label_signatures).
    // The questions is if iterating over the set or hash set is efficient
    // (and produces the same result, because we would then very probably
    // settle for different 'canonical labels' because the ordering would be
    // lost).
    std::unordered_map<LabelSignature, bool> is_label_reduced;
    std::vector<LabelSignature> reduced_label_signatures;

    for (int i = 0; i < labels.size(); ++i) {
        Label *label = labels[i];
        if (label->is_reduced() || (i < global_simulation_task()->get_num_operators() && is_dead(i))) {
            // ignore already reduced labels
            continue;
        }
        ++num_labels;
        LabelSignature signature = build_label_signature(*label, var_is_used);

        if (!reduced_label_map.count(signature)) {
            is_label_reduced[signature] = false;
            ++num_labels_after_reduction;
        } else {
            assert(is_label_reduced.count(signature));
            if (!is_label_reduced[signature]) {
                is_label_reduced[signature] = true;
                reduced_label_signatures.push_back(signature);
            }
        }
        reduced_label_map[signature].push_back(label);
    }
    assert(reduced_label_map.size() == num_labels_after_reduction);

    for (const auto &signature : reduced_label_signatures) {
        const std::vector<Label *> &reduced_labels = reduced_label_map[signature];
        Label *new_label = new CompositeLabel(labels.size(), reduced_labels);
        labels.push_back(new_label);
    }

    std::cout << "Old, local label reduction: "
              << num_labels << " labels, "
              << num_labels_after_reduction << " after reduction"
              << std::endl;
    return num_labels - num_labels_after_reduction;
}

EquivalenceRelation *LabelReducer::compute_outside_equivalence(int abs_index,
                                                               const std::vector<Abstraction *> &all_abstractions,
                                                               const std::vector<Label *> &labels,
                                                               std::vector<EquivalenceRelation *> &local_equivalence_relations) {
    /*Returns an equivalence relation over labels s.t. l ~ l'
    iff l and l' are locally equivalent in all transition systems
    T' \neq T. (They may or may not be locally equivalent in T.)
    Here: T = abstraction. */
    Abstraction *abstraction = all_abstractions[abs_index];
    assert(abstraction);
    //cout << abstraction->tag() << "compute combinable labels" << endl;

    // We always normalize the "starting" abstraction and delete the cached
    // local equivalence relation (if exists) because this does not happen
    // in the refinement loop below.
    abstraction->normalize();
    if (local_equivalence_relations[abs_index]) {
        delete local_equivalence_relations[abs_index];
        local_equivalence_relations[abs_index] = nullptr;
    }

    // create the equivalence relation where all labels are equivalent
    int num_labels = labels.size();
    std::vector<std::pair<int, int>> annotated_labels;
    annotated_labels.reserve(num_labels);
    for (int label_no = 0; label_no < num_labels; ++label_no) {
        const Label *label = labels[label_no];
        assert(label->get_id() == label_no);
        if (!label->is_reduced()) {
            // only consider non-reduced labels
            annotated_labels.emplace_back(0, label_no);
        }
    }
    EquivalenceRelation *relation = EquivalenceRelation::from_annotated_elements<int>(num_labels, annotated_labels);

    for (std::size_t i = 0; i < all_abstractions.size(); ++i) {
        Abstraction *abs = all_abstractions[i];
        if (!abs || abs == abstraction) {
            continue;
        }
        if (!abs->is_normalized()) {
            abs->normalize();
            if (local_equivalence_relations[i]) {
                delete local_equivalence_relations[i];
                local_equivalence_relations[i] = nullptr;
            }
        }
        //cout << abs->tag();
        if (!local_equivalence_relations[i]) {
            //cout << "compute local equivalence relation" << endl;
            local_equivalence_relations[i] = abs->compute_local_equivalence_relation();
        } else {
            //cout << "use cached local equivalence relation" << endl;
            assert(abs->is_normalized());
        }
        relation->refine(*local_equivalence_relations[i]);
    }
    return relation;
}

bool LabelReducer::reduce_exactly(const EquivalenceRelation *relation, std::vector<Label *> &labels) {
    int num_labels = 0;
    int num_labels_after_reduction = 0;
    for (const auto &block : *relation) {
        std::vector<Label *> equivalent_labels;
        for (int jt : block) {
            assert(jt < labels.size());
            Label *label = labels[jt];
            if (!label->is_reduced()) {
                // only consider non-reduced labels
                equivalent_labels.push_back(label);
                ++num_labels;
            }
        }
        if (equivalent_labels.size() > 1) {
            Label *new_label = new CompositeLabel(labels.size(), equivalent_labels);
            labels.push_back(new_label);
        }
        if (!equivalent_labels.empty()) {
            ++num_labels_after_reduction;
        }
    }
    int number_reduced_labels = num_labels - num_labels_after_reduction;
    if (number_reduced_labels > 0) {
        std::cout << "Label reduction: "
                  << num_labels << " labels, "
                  << num_labels_after_reduction << " after reduction"
                  << std::endl;
    }
    return number_reduced_labels;
}

void LabelReducer::dump_options() const {
    std::cout << "Label reduction: ";
    switch (label_reduction_method) {
    case LabelReductionMethod::NONE:
        std::cout << "disabled";
        break;
    case LabelReductionMethod::OLD:
        std::cout << "old";
        break;
    case LabelReductionMethod::TWO_ABSTRACTIONS:
        std::cout << "two abstractions (which will be merged next)";
        break;
    case LabelReductionMethod::ALL_ABSTRACTIONS:
        std::cout << "all abstractions";
        break;
    case LabelReductionMethod::ALL_ABSTRACTIONS_WITH_FIXPOINT:
        std::cout << "all abstractions with fixpoint computation";
        break;
    }
    std::cout << std::endl;
    if (label_reduction_method == LabelReductionMethod::ALL_ABSTRACTIONS ||
        label_reduction_method == LabelReductionMethod::ALL_ABSTRACTIONS_WITH_FIXPOINT) {
        std::cout << "System order: ";
        switch (label_reduction_system_order) {
        case LabelReductionSystemOrder::REGULAR:
            std::cout << "regular";
            break;
        case LabelReductionSystemOrder::REVERSE:
            std::cout << "reversed";
            break;
        case LabelReductionSystemOrder::RANDOM:
            std::cout << "random";
            break;
        }
        std::cout << std::endl;
    }

    std::cout << "max time for label reduction: " << max_time << std::endl;
}

bool LabelReducer::applies_perfect_label_reduction() const {
    return label_reduction_method == LabelReductionMethod::ALL_ABSTRACTIONS_WITH_FIXPOINT;
}

static plugins::TypedEnumPlugin<LabelReducer::LabelReductionMethod> _enum_plugin1({
        {"NONE", ""},
        {"OLD", ""},
        {"TWO_ABSTRACTIONS", ""},
        {"ALL_ABSTRACTIONS", ""},
        {"ALL_ABSTRACTIONS_WITH_FIXPOINT", ""}
    });

static plugins::TypedEnumPlugin<LabelReducer::LabelReductionSystemOrder> _enum_plugin2({
        {"REGULAR", ""},
        {"REVERSE", ""},
        {"RANDOM", ""}
    });
}
