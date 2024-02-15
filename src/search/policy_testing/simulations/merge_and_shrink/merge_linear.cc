#include "merge_linear.h"

#include "../../../plugins/plugin.h"
#include "abstraction.h"

#include <cassert>

namespace simulations {
MergeLinear::MergeLinear(const plugins::Options &opts)
    : MergeStrategy(),
      order(opts.get<VariableOrderType>("variable_order")),
      need_first_index(true) {
}

// Alvaro: Merge strategies have now a limit on the size of the merge.
// If specified (> 0), the pair returned should fit the constraint:
// a1.size()*a2.size()<=limit
std::pair<int, int> MergeLinear::get_next(const std::vector<Abstraction *> &all_abstractions,
                                          int limit_abstract_states_merge, int /*min_limit_abstract_states_merge*/,
                                          int /*limit_transitions_merge*/) {
    assert(!done() && !order.done());

    int first;
    if (need_first_index) {
        need_first_index = false;
        first = order.next();
        //This may happen if some variables are eliminated due to being irrelevant
        while (!all_abstractions[first] && !done() && !order.done()) {
            first = order.next();
            remaining_merges--;
        }
        if (!all_abstractions[first]) {
            return std::make_pair(-1, -1);
        }

        std::cout << "First variable: " << first << std::endl;
    } else {
        // The most recent composite abstraction is appended at the end of
        // all_abstractions in merge_and_shrink.cc
        first = all_abstractions.size() - 1;
    }
    int second = order.next();

    while (!all_abstractions[second] && !done() && !order.done()) {
        std::cout << "Skipping var " << second << std::endl;
        second = order.next();
        remaining_merges--;
    }
    if (!all_abstractions[second]) {
        return std::make_pair(-1, -1);
    }

    std::cout << "Next variable: " << second << std::endl;
    assert(all_abstractions[first]);
    assert(all_abstractions[second]);
    --remaining_merges;
    if (done() && !order.done()) {
        std::cerr << "Variable order finder not done, but no merges remaining" << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
    }
    if (limit_abstract_states_merge &&
        all_abstractions[first]->size() * all_abstractions[second]->size() > limit_abstract_states_merge) {
        if (!done()) {
            return get_next(all_abstractions, limit_abstract_states_merge);
        } else {
            return std::make_pair(-1, -1);
        }
    }
    return std::make_pair(first, second);
}

void MergeLinear::dump_strategy_specific_options() const {
    std::cout << "Linear merge strategy: ";
    order.dump();
}

std::string MergeLinear::name() const {
    return "linear";
}

bool MergeLinear::is_linear() const {
    return true;
}

class MergeLinearFeature : public plugins::TypedFeature<MergeStrategy, MergeLinear> {
public:
    MergeLinearFeature() : TypedFeature("merge_linear") {
        add_option<VariableOrderType>("variable_order", "the order in which atomic abstractions are merged",
                                      "CG_GOAL_LEVEL");
    }
};

static plugins::FeaturePlugin<MergeLinearFeature> _plugin;
}
