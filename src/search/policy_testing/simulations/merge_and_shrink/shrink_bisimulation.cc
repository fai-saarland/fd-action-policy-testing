#include "shrink_bisimulation.h"

#include "abstraction.h"
#include "../../../plugins/plugin.h"

#include <cassert>
#include <iostream>
#include <limits>
#include <unordered_map>
#include <utility>

namespace simulations {
/* A successor signature characterizes the behaviour of an abstract
   state in so far as bisimulation cares about it. States with
   identical successor signature are not distinguished by
   bisimulation.

   Each entry in the vector is a pair of (label, equivalence class of
   successor). The bisimulation algorithm requires that the vector is
   sorted and uniquified. */

typedef std::vector<std::pair<int, int>> SuccessorSignature;

/* TODO: The following class should probably be renamed. It encodes
   all we need to know about a state for bisimulation: its h value,
   which equivalence class ("group") it currently belongs to, its
   signature (see above), and what the original state is. */

struct Signature {
    int h_and_goal;     // -1 for goal states; h value for non-goal states
    int group;
    SuccessorSignature succ_signature;
    int state;

    Signature(int h, bool is_goal, int group_,
              SuccessorSignature succ_signature_,
              int state_)
        : group(group_), succ_signature(std::move(succ_signature_)), state(state_) {
        if (is_goal) {
            assert(h == 0);
            h_and_goal = -1;
        } else {
            h_and_goal = h;
        }
    }

    bool operator<(const Signature &other) const {
        if (h_and_goal != other.h_and_goal)
            return h_and_goal < other.h_and_goal;
        if (group != other.group)
            return group < other.group;
        if (succ_signature != other.succ_signature)
            return succ_signature < other.succ_signature;
        return state < other.state;
    }

    void dump() const {
        std::cout << "Signature(h_and_goal = " << h_and_goal
                  << ", group = " << group
                  << ", state = " << state
                  << ", succ_sig = [";
        for (size_t i = 0; i < succ_signature.size(); ++i) {
            if (i)
                std::cout << ", ";
            std::cout << "(" << succ_signature[i].first
                      << "," << succ_signature[i].second
                      << ")";
        }
        std::cout << "])" << std::endl;
    }
};


// TODO: This is a general tool that probably belongs somewhere else.
template<class T>
void release_memory(std::vector <T> &vec) {
    std::vector<T>().swap(vec);
}

ShrinkBisimulation::ShrinkBisimulation(const plugins::Options &opts)
    : ShrinkStrategy(opts),
      greedy(opts.get<bool>("greedy")),
      threshold(opts.get<int>("threshold")),
      group_by_h(opts.get<bool>("group_by_h")),
      at_limit(opts.get<AtLimit>("at_limit")),
      aggregate_goals(opts.get<bool>("aggregate_goals")) {
    if (threshold == -1) {
        threshold = opts.get<int>("max_states");
    }
    if (threshold < 1) {
        std::cerr << "bisimulation threshold must be at least 1" << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
    }
    if (threshold > opts.get<int>("max_states")) {
        std::cerr << "bisimulation threshold must not be larger than size limit" << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
    }
}

ShrinkBisimulation::~ShrinkBisimulation() = default;

std::string ShrinkBisimulation::name() const {
    return "bisimulation";
}

void ShrinkBisimulation::dump_strategy_specific_options() const {
    std::cout << "Bisimulation type: " << (greedy ? "greedy" : "exact") << std::endl;
    std::cout << "Bisimulation threshold: " << threshold << std::endl;
    std::cout << "Group by h: " << (group_by_h ? "yes" : "no") << std::endl;
    std::cout << "At limit: ";
    if (at_limit == AtLimit::RETURN) {
        std::cout << "return";
    } else if (at_limit == AtLimit::USE_UP) {
        std::cout << "use up limit";
    } else {
        ABORT("Unknown setting for at_limit.");
    }
    std::cout << "Aggregate goals: " << (aggregate_goals ? "yes" : "no") << std::endl;

    std::cout << std::endl;
}

bool ShrinkBisimulation::reduce_labels_before_shrinking() const {
    return true;
}

void ShrinkBisimulation::shrink(Abstraction &abs, int target, bool force) {
    // TODO: Explain this min(target, threshold) stuff. Also, make the
    //       output clearer, which right now is rubbish, calling the
    //       min(...) "threshold". The reasoning behind this is that
    //       we need to shrink if we're above the threshold or if
    //       *after* composition we'd be above the size limit, so
    //       target can either be less or larger than threshold.
    if (must_shrink(abs, std::min(target, threshold), force)) {
        EquivalenceRelation equivalence_relation;
        compute_abstraction(abs, target, equivalence_relation);
        apply(abs, equivalence_relation, target);
    }
}

void ShrinkBisimulation::shrink_atomic(Abstraction &abs) {
    // Perform an exact bisimulation on all atomic abstractions.

    // TODO/HACK: Come up with a better way to do this than generating
    // a new shrinking class instance in this roundabout fashion. We
    // shouldn't need to generate a new instance at all.

    int old_size = abs.size();
    std::shared_ptr<ShrinkStrategy> strategy = create_default();
    strategy->shrink(abs, abs.size(), true);
    // delete strategy;
    if (abs.size() != old_size) {
        std::cout << "Atomic abstraction simplified "
                  << "from " << old_size
                  << " to " << abs.size()
                  << " states." << std::endl;
    }
}

void ShrinkBisimulation::shrink_before_merge(
    Abstraction &abs1, Abstraction &abs2) {
    std::pair<int, int> new_sizes = compute_shrink_sizes(abs1.size(), abs2.size());
    int new_size1 = new_sizes.first;
    int new_size2 = new_sizes.second;

    shrink(abs2, new_size2);
    shrink(abs1, new_size1);
}

int ShrinkBisimulation::initialize_groups(const Abstraction &abs, std::vector<int> &state_to_group) {
    /* Group 0 holds all goal states.

       Each other group holds all states with one particular h value.

       Note that some goal state *must* exist because irrelevant und
       unreachable states are pruned before we shrink and we never
       perform the shrinking if that pruning shows that the problem is
       unsolvable.
    */

    using GroupMap = std::unordered_map<int, int>;
    GroupMap h_to_group;
    int num_groups = 1;     // Group 0 is for goal states.
    for (int state = 0; state < abs.size(); ++state) {
        int h = abs.get_goal_distance(state);
        assert(h >= 0 && h != PLUS_INFINITY);
        assert(abs.get_init_distance(state) != PLUS_INFINITY);

        if (abs.is_goal_state(state)) {
            assert(h == 0);
            state_to_group[state] = 0;
        } else {
            std::pair<GroupMap::iterator, bool> result = h_to_group.insert(std::make_pair(h, num_groups));
            state_to_group[state] = result.first->second;
            if (result.second) {
                // We inserted a new element => a new group was started.
                ++num_groups;
            }
        }
    }
    return num_groups;
}

void ShrinkBisimulation::compute_signatures(
    const Abstraction &abs,
    std::vector <Signature> &signatures,
    std::vector<int> &state_to_group) const {
    assert(signatures.empty());

    // Step 1: Compute bare state signatures (without transition information).
    signatures.emplace_back(-2, false, -1, SuccessorSignature(), -1);
    for (int state = 0; state < abs.size(); ++state) {
        int h = abs.get_goal_distance(state);
        assert(h >= 0 && h <= abs.get_max_h());
        Signature signature(h, abs.is_goal_state(state),
                            state_to_group[state], SuccessorSignature(),
                            state);
        signatures.push_back(signature);
    }
    signatures.emplace_back(PLUS_INFINITY, false, -1, SuccessorSignature(), -1);

    // Step 2: Add transition information.
    int num_labels = abs.get_num_labels();
    for (int label_no = 0; label_no < num_labels; ++label_no) {
        const std::vector<AbstractTransition> &transitions =
            abs.get_transitions_for_label(label_no);
        int label_cost = abs.get_label_cost_by_index(label_no);
        for (const auto &trans : transitions) {
            assert(signatures[trans.src + 1].state == trans.src);
            bool skip_transition = false;
            if (greedy) {
                int src_h = abs.get_goal_distance(trans.src);
                int target_h = abs.get_goal_distance(trans.target);
                assert(target_h + label_cost >= src_h);
                skip_transition = (target_h + label_cost != src_h);
            }
            if (aggregate_goals && abs.get_all_goal_vars_in()
                && abs.is_goal_state(trans.src)) {
                skip_transition = true;
            }
            if (!skip_transition) {
                int target_group = state_to_group[trans.target];
                signatures[trans.src + 1].succ_signature.emplace_back(label_no, target_group);
            }
        }
    }

    /* Step 3: Canonicalize the representation. The resulting
       signatures must satisfy the following properties:

       1. Signature::operator< defines a total order with the correct
          sentinels at the start and end. The signatures vector is
          sorted according to that order.
       2. Goal states come before non-goal states, and low-h states come
          before high-h states.
       3. States that currently fall into the same group form contiguous
          subsequences.
       4. Two signatures compare equal according to Signature::operator<
          iff we don't want to distinguish their states in the current
          bisimulation round.
     */

    for (auto &signature : signatures) {
        SuccessorSignature &succ_sig = signature.succ_signature;
        std::sort(succ_sig.begin(), succ_sig.end());
        succ_sig.erase(std::unique(succ_sig.begin(), succ_sig.end()), succ_sig.end());
    }

    std::sort(signatures.begin(), signatures.end());
    /* TODO: Should we sort an index set rather than shuffle the whole
       signatures around? But since swapping vectors is fast, we
       probably don't have to worry about that. */
}

void ShrinkBisimulation::compute_abstraction(
    Abstraction &abs,
    int target_size,
    EquivalenceRelation &equivalence_relation) {
    int num_states = abs.size();

    std::vector<int> state_to_group(num_states);
    std::vector<Signature> signatures;
    signatures.reserve(num_states + 2);

    int num_groups = initialize_groups(abs, state_to_group);
    // cout << "number of initial groups: " << num_groups << endl;

    // assert(num_groups <= target_size); // TODO: We currently violate this; see issue250

    int max_h = abs.get_max_h();
    assert(max_h >= 0 && max_h != PLUS_INFINITY);

    bool stable = false;
    bool stop_requested = false;
    while (!stable && !stop_requested && num_groups < target_size) {
        stable = true;

        signatures.clear();
        compute_signatures(abs, signatures, state_to_group);

        // Verify size of signatures and presence of sentinels.
        assert(signatures.size() == num_states + 2);
        assert(signatures[0].h_and_goal == -2);
        assert(signatures[num_states + 1].h_and_goal == PLUS_INFINITY);

        int sig_start = 1;     // Skip over initial sentinel.
        while (true) {
            int h_and_goal = signatures[sig_start].h_and_goal;
            int group = signatures[sig_start].group;
            if (h_and_goal > max_h) {
                // We have hit the end sentinel.
                assert(h_and_goal == PLUS_INFINITY);
                assert(sig_start + 1 == signatures.size());
                break;
            }

            // Compute the number of groups needed after splitting.
            int num_old_groups = 0;
            int num_new_groups = 0;
            int sig_end;
            for (sig_end = sig_start; true; ++sig_end) {
                if (group_by_h) {
                    if (signatures[sig_end].h_and_goal != h_and_goal)
                        break;
                } else {
                    if (signatures[sig_end].group != group)
                        break;
                }

                const Signature &prev_sig = signatures[sig_end - 1];
                const Signature &curr_sig = signatures[sig_end];

                if (sig_end == sig_start)
                    assert(prev_sig.group != curr_sig.group);

                if (prev_sig.group != curr_sig.group) {
                    ++num_old_groups;
                    ++num_new_groups;
                } else if (prev_sig.succ_signature != curr_sig.succ_signature) {
                    ++num_new_groups;
                }
            }
            assert(sig_end > sig_start);

            if (at_limit == AtLimit::RETURN &&
                num_groups - num_old_groups + num_new_groups > target_size) {
                /* Can't split the group (or the set of groups for
                   this h value) -- would exceed bound on abstract
                   state number.
                */
                stop_requested = true;
                break;
            } else if (num_new_groups != num_old_groups) {
                // Split into new groups.
                stable = false;

                int new_group_no = -1;
                for (size_t i = sig_start; i < sig_end; ++i) {
                    const Signature &prev_sig = signatures[i - 1];
                    const Signature &curr_sig = signatures[i];

                    if (prev_sig.group != curr_sig.group) {
                        // Start first group of a block; keep old group no.
                        new_group_no = curr_sig.group;
                    } else if (prev_sig.succ_signature
                               != curr_sig.succ_signature) {
                        new_group_no = num_groups++;
                        assert(num_groups <= target_size);
                    }

                    assert(new_group_no != -1);
                    state_to_group[curr_sig.state] = new_group_no;
                    if (num_groups == target_size)
                        break;
                }
                if (num_groups == target_size)
                    break;
            }
            sig_start = sig_end;
        }
    }

    /* Reduce memory pressure before generating the equivalence
       relation since this is one of the code parts relevant to peak
       memory. */
    release_memory(signatures);

    // Generate final result.
    assert(equivalence_relation.empty());
    equivalence_relation.resize(num_groups);
    for (int state = 0; state < num_states; ++state) {
        int group = state_to_group[state];
        if (group != -1) {
            assert(group >= 0 && group < num_groups);
            equivalence_relation[group].push_front(state);
        }
    }
}

std::shared_ptr<ShrinkStrategy> ShrinkBisimulation::create_default(bool aggregate_goals, int limit_states) {
    plugins::Options opts;
    opts.set("max_states", limit_states);
    opts.set("max_states_before_merge", limit_states);
    opts.set<bool>("greedy", false);
    opts.set("threshold", (limit_states == PLUS_INFINITY) ? 1 : limit_states);

    opts.set("group_by_h", limit_states != PLUS_INFINITY);
    opts.set<AtLimit>("at_limit", AtLimit::RETURN);
    opts.set<bool>("aggregate_goals", aggregate_goals);

    return std::make_shared<ShrinkBisimulation>(opts);
}


class ShrinkBisimulationFeature : public plugins::TypedFeature<ShrinkStrategy, ShrinkBisimulation> {
public:
    ShrinkBisimulationFeature() : TypedFeature("sim_shrink_bisimulation") {
        ShrinkStrategy::add_options_to_feature(*this);
        add_option<bool>("greedy", "use greedy bisimulation", "false");
        add_option<int>("threshold", "TODO: document", "-1");      // default: same as max_states
        add_option<bool>("group_by_h", "TODO: document", "false");
        add_option<ShrinkBisimulation::AtLimit>("at_limit", "what to do when the size limit is hit", "RETURN");
        add_option<bool>("aggregate_goals", "Goal states in abstractions"
                         "with all goal variables will always remain goal"
                         "-> we can ignore all outgoing transitions, as we can"
                         "never leave this state; should help aggregating more states, as"
                         "all goal states should become bisimilar (can also increase the"
                         "abstraction size by making more abstract states reachable)",
                         "false");
    }
};

static plugins::FeaturePlugin<ShrinkBisimulationFeature> _plugin;

static plugins::TypedEnumPlugin<ShrinkBisimulation::AtLimit> _enum_plugin({
        {"RETURN", ""},
        {"USE_UP", ""}
    });
}
