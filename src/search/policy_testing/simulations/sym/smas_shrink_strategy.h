#pragma once

#include <string>
#include <vector>
#include <list>

#include "../../../option_parser.h"

namespace simulations {
class SymSMAS;

using AbstractStateRef = int;
using AbstractStateRefList = std::list<AbstractStateRef>;

class SMASShrinkStrategy {
    const int max_states;
    const int max_states_before_merge;
    const int max_trs;
protected:
    /* An equivalence class is a set of abstract states that shall be
       mapped (shrunk) to the same abstract state.

       An equivalence relation is a partitioning of states into
       equivalence classes. It may omit certain states entirely; these
       will be dropped completely and receive an h value of infinity.
       This is used to remove unreachable and irrelevant states.
    */

    using EquivalenceClass = AbstractStateRefList;
    using EquivalenceRelation = std::vector<EquivalenceClass>;

public:
    // HACK/TODO: The following method would usually be protected, but
    // the option parser requires it to be public for the
    // DefaultValueNamer. We need to reconsider the use of the
    // DefaultValueNamer here anyway, along with the way the default
    // strategy is specified. Maybe add a capability to have the default
    // value be a factory function with a string description?
    [[nodiscard]] virtual std::string name() const = 0;

protected:
    virtual void dump_strategy_specific_options() const;

    [[nodiscard]] std::pair<int, int> compute_shrink_sizes(int size1, int size2,
                                                           int trs1, int trs2) const;

    [[nodiscard]] static bool must_shrink(const SymSMAS &abs, int threshold, bool force);

    static void apply(SymSMAS &abs,
                      EquivalenceRelation &equivalence_relation,
                      int threshold);

public:
    explicit SMASShrinkStrategy(const options::Options &opts);

    virtual ~SMASShrinkStrategy() = default;

    /* Set this to true to apply label reduction before shrinking, in
       addition to the times when it is usually applied. Some shrink
       strategies may require labels to be reduced (e.g. ones based on
       bisimulations), and others might prefer reduced labels because
       it makes their computation more efficient.

       Set this to false if the shrink strategy does not benefit (or
       not much) from label reduction. Label reduction costs quite a
       bit of time and should not be performed without need.
    */
    [[nodiscard]] virtual bool reduce_labels_before_shrinking() const = 0;

    void dump_options() const;

    /* TODO: Make sure that *all* shrink strategies prune irrelevant
       and unreachable states, then update documentation below
       accordingly? Currently the only exception is ShrinkRandom, I
       think. */

    /* Shrink the abstraction to size threshold.

       In most shrink stategies, this also prunes all irrelevant and
       unreachable states, which may cause the resulting size to be
       lower than threshold.

       Does nothing if threshold >= abs.size() unless force is true
       (in which case it only prunes irrelevant and unreachable
       states).
    */

    // TODO: Should all three of these be virtual?
    // TODO: Should all three of these be public?
    //       If not, also modify in derived clases.

    virtual bool shrink(SymSMAS &abs, int threshold, bool force = false) = 0;

    virtual void shrink_atomic(SymSMAS &abs1);

    virtual bool shrink_before_merge(SymSMAS &abs1, SymSMAS &abs2);


    static void add_options_to_parser(options::OptionParser &parser);

    static void handle_option_defaults(options::Options &opts);
};
}
