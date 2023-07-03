#include "shrink_strategy.h"

#include "abstraction.h"

#include "../../../plugin.h"
#include "../utils/debug.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

namespace simulations {
ShrinkStrategy::ShrinkStrategy(const options::Options &opts)
    : max_states(opts.get<int>("max_states")),
      max_states_before_merge(opts.get<int>("max_states_before_merge")) {
    assert(max_states_before_merge > 0);
    assert(max_states >= max_states_before_merge);
}

void ShrinkStrategy::dump_options() const {
    std::cout << "Shrink strategy: " << name() << std::endl;
    std::cout << "Abstraction size limit: " << max_states << std::endl
              << "Abstraction size limit right before merge: "
              << max_states_before_merge << std::endl;
    dump_strategy_specific_options();
}

void ShrinkStrategy::dump_strategy_specific_options() const {
    // Default implementation does nothing.
}

bool ShrinkStrategy::must_shrink(
    const Abstraction &abs, int threshold, bool force) const {
    assert(threshold >= 1);
    assert(abs.is_solvable());
    if (abs.size() > threshold) {
        DEBUG_MAS(std::cout << abs.tag() << "shrink from size " << abs.size()
                            << " (threshold: " << threshold << ")" << std::endl;
                  );
        return true;
    }
    if (force) {
        DEBUG_MAS(std::cout << abs.tag() << "shrink forced to prune unreachable/irrelevant states" << std::endl;
                  );
        return true;
    }
    return false;
}

std::pair<int, int> ShrinkStrategy::compute_shrink_sizes(
    int size1, int size2) const {
    // Bound both sizes by max allowed size before merge.
    int new_size1 = std::min(size1, max_states_before_merge);
    int new_size2 = std::min(size2, max_states_before_merge);

    // Check if product would exceed max allowed size.
    // Use division instead of multiplication to avoid overflow.
    if (max_states / new_size1 < new_size2) {
        int balanced_size = int(sqrt(max_states));

        if (new_size1 <= balanced_size) {
            // Size of the first abstraction is small enough. Use whatever
            // is left for the second abstraction.
            new_size2 = max_states / new_size1;
        } else if (new_size2 <= balanced_size) {
            // Inverted case as before.
            new_size1 = max_states / new_size2;
        } else {
            // Both abstractions are too big. We set both target sizes
            // to balanced_size. An alternative would be to set one to
            // N1 = balanced_size and the other to N2 = max_states /
            // balanced_size, to get closer to the allowed maximum.
            // However, this would make little difference (N2 would
            // always be N1, N1 + 1 or N1 + 2), and our solution has the
            // advantage of treating the abstractions symmetrically.
            new_size1 = balanced_size;
            new_size2 = balanced_size;
        }
    }
    assert(new_size1 <= size1 && new_size2 <= size2);
    assert(new_size1 <= max_states_before_merge);
    assert(new_size2 <= max_states_before_merge);
    assert(new_size1 * new_size2 <= max_states);
    return std::make_pair(new_size1, new_size2);
}

void ShrinkStrategy::shrink_atomic(Abstraction & /*abs*/) {
    // Default implemention does nothing.
}

void ShrinkStrategy::shrink_before_merge(Abstraction &abs1, Abstraction &abs2) {
    std::pair<int, int> new_sizes = compute_shrink_sizes(abs1.size(), abs2.size());
    int new_size1 = new_sizes.first;
    int new_size2 = new_sizes.second;
    if (new_size2 != abs2.size()) {
        shrink(abs2, new_size2);
    }

    if (new_size1 != abs1.size()) {
        shrink(abs1, new_size1);
    }
}

/*
  TODO: I think we could get a nicer division of responsibilities if
  this method were part of the abstraction class. The shrink
  strategies would then return generate an equivalence class
  ("collapsed_groups") and not modify the abstraction, which would be
  passed as const.
 */

void ShrinkStrategy::apply(
    Abstraction &abs,
    EquivalenceRelation &equivalence_relation,
    int target) const {
    // TODO: We currently violate this; see issue250
    //assert(equivalence_relation.size() <= target);
    abs.apply_abstraction(equivalence_relation);
    std::cout << abs.tag() << "size after shrink " << abs.size()
              << ", target " << target << std::endl;
    //assert(abs.size() <= target);
}

void ShrinkStrategy::add_options_to_parser(OptionParser &parser) {
    // TODO: better documentation what each parameter does
    parser.add_option<int>(
        "max_states",
        "maximum abstraction size", "-1");
    parser.add_option<int>(
        "max_states_before_merge",
        "maximum abstraction size for factors of synchronized product", "-1");
}

void ShrinkStrategy::handle_option_defaults(Options &opts) {
    int max_states = -1;
    int max_states_before_merge = -1;
    if (opts.contains("max_states")) {
        max_states = opts.get<int>("max_states");
    }
    if (opts.contains("max_states_before_merge")) {
        max_states_before_merge = opts.get<int>("max_states_before_merge");
    }
    if (max_states == -1 && max_states_before_merge == -1) {
        // None of the two options specified: set default limit.
        max_states = 50000;
    }

    // If exactly one of the max_states options has been set, set the other
    // so that it imposes no further limits.
    if (max_states_before_merge == -1) {
        max_states_before_merge = max_states;
    } else if (max_states == -1) {
        int n = max_states_before_merge;
        max_states = n * n;
        if (max_states < 0 || max_states / n != n)             // overflow
            max_states = std::numeric_limits<int>::max();
    }

    if (max_states_before_merge > max_states) {
        std::cerr << "warning: max_states_before_merge exceeds max_states, "
                  << "correcting." << std::endl;
        max_states_before_merge = max_states;
    }

    if (max_states < 1) {
        std::cerr << "error: abstraction size must be at least 1" << std::endl;
        exit_with(EXIT_INPUT_ERROR);
    }

    if (max_states_before_merge < 1) {
        std::cerr << "error: abstraction size before merge must be at least 1"
                  << std::endl;
        exit_with(EXIT_INPUT_ERROR);
    }

    opts.set<int>("max_states", max_states);
    opts.set<int>("max_states_before_merge", max_states_before_merge);
}


static std::shared_ptr<ShrinkStrategy> _parse(OptionParser & /*parser*/) {
    return nullptr;
}

static PluginTypePlugin<ShrinkStrategy> _plugin_type_none("shrink_strategy", "");
static Plugin<ShrinkStrategy> _plugin("shrink_strategy_none", _parse);
}
