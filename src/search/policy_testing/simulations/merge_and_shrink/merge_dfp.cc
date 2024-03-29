#include "merge_dfp.h"

#include "abstraction.h"
#include "label.h"

#include "../../../plugins/plugin.h"
#include "../utils/debug.h"

#include <cassert>
#include <iostream>

namespace simulations {
MergeDFP::MergeDFP() : MergeStrategy() {
    add_init_function([&]() {
                          assert(border_atomics_composites == 0);
                          initialized = true;
                          border_atomics_composites = global_simulation_task()->get_num_variables();
                      });
    // n := global_simulation_task()->get_num_variables() is the number of variables of the planning task
    // and thus the number of atomic abstractions. These will be stored at
    // indices 0 to n-1 and thus n is the index at which the first composite
    // abstraction will be stored at.
}


bool check_valid_size(Abstraction *abstraction, Abstraction *other_abstraction,
                      int limit_abstract_states_merge, int min_limit_abstract_states_merge,
                      int limit_transitions_merge) {
    if (limit_abstract_states_merge) {
        if (abstraction->size() * other_abstraction->size() > limit_abstract_states_merge) {
            return false;
        }
    }

    if (min_limit_abstract_states_merge) {
        if (abstraction->size() * other_abstraction->size() <= min_limit_abstract_states_merge) {
            return true;
        }
    }

    if (limit_transitions_merge) {
        if (abstraction->estimate_transitions(other_abstraction) > limit_transitions_merge) {
            return false;
        }
    }

    return true;
}

void MergeDFP::init_strategy(const std::vector<Abstraction *> & /*abstractions*/) {
}

size_t MergeDFP::get_corrected_index(int index) const {
    // This method assumes that we iterate over the vector of all
    // abstractions in inverted order (from back to front). It returns the
    // unmodified index as long as we are in the range of composite
    // abstractions (these are thus traversed in order from the last one
    // to the first one) and modifies the index otherwise so that the order
    // in which atomic abstractions are considered is from the first to the
    // last one (from front to back). This is to emulate the previous behavior
    // when new abstractions were not inserted after existing abstractions,
    // but rather replaced arbitrarily one of the two original abstractions.
    assert(index >= 0);
    assert(initialized);
    if (index >= border_atomics_composites)
        return index;
    return border_atomics_composites - 1 - index;
}

// Alvaro: Merge strategies have now a limit on the size of the
// merge.  If specified (> 0), the pair returned should fit the
// constraint: a1.size()*a2.size()<=limit
std::pair<int, int> MergeDFP::get_next(const std::vector<Abstraction *> &all_abstractions,
                                       int limit_abstract_states_merge, int min_limit_abstract_states_merge,
                                       int limit_transitions_merge) {
    assert(initialized);
    assert(!done());
    std::vector < Abstraction * > sorted_abstractions;
    std::vector<int> indices_mapping;
    std::vector <std::vector<int>> abstraction_label_ranks;
    // Precompute a vector sorted_abstrations which contains all existing
    // abstractions from all_abstractions in the desired order.
    for (int i = all_abstractions.size() - 1; i >= 0; --i) {
        // We iterate from back to front, considering the composite
        // abstractions in the order from "most recently added" (= at the back
        // of the vector) to "first added" (= at border_atomics_composites).
        // Afterwards, we consider the atomic abstractions in the "regular"
        // order from the first one until the last one. See also explanation
        // at get_corrected_index().
        size_t abs_index = get_corrected_index(i);
        Abstraction *abstraction = all_abstractions[abs_index];
        if (abstraction) {
            sorted_abstractions.push_back(abstraction);
            indices_mapping.push_back(abs_index);
            abstraction_label_ranks.emplace_back();
            std::vector<int> &label_ranks = abstraction_label_ranks[abstraction_label_ranks.size() - 1];
            abstraction->compute_label_ranks(label_ranks);
        }
    }

    int first = -1;
    int second = -1;
    int minimum_weight = PLUS_INFINITY;
    for (size_t abs_index = 0; abs_index < sorted_abstractions.size(); ++abs_index) {
        Abstraction *abstraction = sorted_abstractions[abs_index];
        assert(abstraction);
        std::vector<int> &label_ranks = abstraction_label_ranks[abs_index];
        assert(!label_ranks.empty());
        for (size_t other_abs_index = abs_index + 1; other_abs_index < sorted_abstractions.size();
             ++other_abs_index) {
            Abstraction *other_abstraction = sorted_abstractions[other_abs_index];
            assert(other_abstraction);
            //Alvaro: Added additional condition to check the size of the product
            if (check_valid_size(abstraction, other_abstraction,
                                 limit_abstract_states_merge, min_limit_abstract_states_merge,
                                 limit_transitions_merge)) {
                if (abstraction->is_goal_relevant() || other_abstraction->is_goal_relevant()) {
                    std::vector<int> &other_label_ranks = abstraction_label_ranks[other_abs_index];
                    assert(!other_label_ranks.empty());
                    assert(label_ranks.size() == other_label_ranks.size());
                    int pair_weight = PLUS_INFINITY;
                    for (size_t i = 0; i < label_ranks.size(); ++i) {
                        if (label_ranks[i] != -1 && other_label_ranks[i] != -1) {
                            // label is relevant in both abstractions
                            int max_label_rank = std::max(label_ranks[i], other_label_ranks[i]);
                            pair_weight = std::min(pair_weight, max_label_rank);
                        }
                    }
                    if (pair_weight < minimum_weight) {
                        minimum_weight = pair_weight;
                        first = indices_mapping[abs_index];
                        second = indices_mapping[other_abs_index];
                        assert(all_abstractions[first] == abstraction);
                        assert(all_abstractions[second] == other_abstraction);
                    }
                }
            }
        }
    }
    if (first == -1) {
        // No pair with finite weight has been found. In this case, we simply
        // take the first pair according to our ordering consisting of at
        // least one goal relevant abstraction.
        assert(second == -1);
        assert(minimum_weight == PLUS_INFINITY);

        for (size_t abs_index = 0; abs_index < sorted_abstractions.size(); ++abs_index) {
            Abstraction *abstraction = sorted_abstractions[abs_index];
            assert(abstraction);
            for (size_t other_abs_index = abs_index + 1; other_abs_index < sorted_abstractions.size();
                 ++other_abs_index) {
                Abstraction *other_abstraction = sorted_abstractions[other_abs_index];
                assert(other_abstraction);
                //Alvaro: Added additional condition to check the size of the product
                if (check_valid_size(abstraction, other_abstraction,
                                     limit_abstract_states_merge, min_limit_abstract_states_merge,
                                     limit_transitions_merge)) {
                    if (abstraction->is_goal_relevant() || other_abstraction->is_goal_relevant()) {
                        first = indices_mapping[abs_index];
                        second = indices_mapping[other_abs_index];
                        assert(all_abstractions[first] == abstraction);
                        assert(all_abstractions[second] == other_abstraction);
                    }
                }
            }
        }
    }
    DEBUG_MAS(std::cout << "Next pair of indices: (" << first << ", " << second << ")" << std::endl;
              );
    --remaining_merges;

    return std::make_pair(first, second);
}

std::string MergeDFP::name() const {
    return "dfp";
}

bool MergeDFP::is_linear() const {
    return false;
}


class MergeDFPFeature : public plugins::TypedFeature<MergeStrategy, MergeDFP> {
public:
    MergeDFPFeature() : TypedFeature("merge_dfp") {
    }
    [[nodiscard]] std::shared_ptr<MergeDFP> create_component(const plugins::Options &, const utils::Context &) const override {
        return std::make_shared<MergeDFP>();
    }
};


static plugins::FeaturePlugin<MergeDFPFeature> _plugin;
}
