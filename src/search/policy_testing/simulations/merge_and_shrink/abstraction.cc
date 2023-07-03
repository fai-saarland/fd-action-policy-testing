#include "abstraction.h"

#include "labelled_transition_system.h"
#include "label.h"
#include "labels.h"
#include "shrink_fh.h"
#include "dominance_relation.h"
#include "simulation_relation.h"
#include "../utils/equivalence_relation.h"
#include "../utils/debug.h"
#include "../utils/priority_queue.h"
#include "../numeric_dominance/int_epsilon.h"
#include "../numeric_dominance/dijkstra_search_epsilon.h"

#include <cassert>
#include <cstring>
#include <deque>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <utility>

namespace simulations {
bool Abstraction::store_original_operators = false;

/* Implementation note: Transitions are grouped by their labels,
   not by source state or any such thing. Such a grouping is beneficial
   for fast generation of products because we can iterate operator by
   operator, and it also allows applying abstraction mappings very
   efficiently.

   We rarely need to be able to efficiently query the successors of a
   given state; actually, only the distance computation requires that,
   and it simply generates such a graph representation of the
   transitions itself. Various experiments have shown that maintaining
   a graph representation permanently for the benefit of distance
   computation is not worth the overhead.
*/

Abstraction::Abstraction(Labels *labels_)
    : labels(labels_), num_labels(labels->get_size()),
      transitions_by_label(
          (global_simulation_task()->get_num_operators() ==
           0) ? 0 : global_simulation_task()->get_num_operators() * 2 - 1),
      relevant_labels(transitions_by_label.size(), false),
      transitions_sorted_unique(true), peak_memory(0),
      simulation_relation(nullptr) {
    if (store_original_operators)
        transitions_by_label_based_on_operators.resize(
            (global_simulation_task()->get_num_operators() ==
             0) ? 0 : global_simulation_task()->get_num_operators() * 2 - 1);

    clear_distances();
}

std::string Abstraction::tag() const {
    std::string desc(description());
    desc[0] = std::toupper(desc[0]);
    return desc + ": ";
}

void Abstraction::clear_distances() {
    max_f = DISTANCE_UNKNOWN;
    max_g = DISTANCE_UNKNOWN;
    max_h = DISTANCE_UNKNOWN;
    init_distances.clear();
    goal_distances.clear();
}

int Abstraction::size() const {
    return num_states;
}

int Abstraction::get_max_f() const {
    return max_f;
}

int Abstraction::get_max_g() const {
    return max_g;
}

int Abstraction::get_max_h() const {
    return max_h;
}

int Abstraction::get_label_cost_by_index(int label_no) const {
    const Label *label = labels->get_label_by_index(label_no);
    return label->get_cost();
}

const std::vector<AbstractTransition> &Abstraction::get_transitions_for_label(int label_no) const {
    return transitions_by_label[label_no];
}

const std::vector<boost::dynamic_bitset<>> &Abstraction::get_transition_ops_for_label(int label_no) const {
    assert(label_no < transitions_by_label_based_on_operators.size());
    return transitions_by_label_based_on_operators[label_no];
}

int Abstraction::get_num_labels() const {
    return labels->get_size();
}

int Abstraction::get_num_nonreduced_labels() const {
    int num = 0;
    for (int l = 0; l < labels->get_size(); l++) {
        if (!labels->is_label_reduced(l)) {
            num++;
        }
    }
    return num;
}

void Abstraction::compute_label_ranks(std::vector<int> &label_ranks) {
    // abstraction needs to be normalized when considering labels and their
    // transitions
    if (!is_normalized()) {
        normalize();
    }
    // distances must be computed
    if (max_h == DISTANCE_UNKNOWN) {
        compute_distances();
    }
    assert(label_ranks.empty());
    label_ranks.reserve(transitions_by_label.size());
    for (size_t label_no = 0; label_no < transitions_by_label.size(); ++label_no) {
        if (relevant_labels[label_no]) {
            const std::vector<AbstractTransition> &transitions = transitions_by_label[label_no];
            int label_rank = PLUS_INFINITY;
            for (const auto &t: transitions) {
                label_rank = std::min(label_rank, goal_distances[t.target]);
            }
            // relevant labels with no transitions have a rank of infinity (they
            // block synchronization)
            label_ranks.push_back(label_rank);
        } else {
            label_ranks.push_back(-1);
        }
    }
}

bool Abstraction::are_distances_computed() const {
    if (max_h == DISTANCE_UNKNOWN) {
        assert(max_f == DISTANCE_UNKNOWN);
        assert(max_g == DISTANCE_UNKNOWN);
        assert(init_distances.empty());
        assert(goal_distances.empty());
        return false;
    }
    return true;
}

void Abstraction::compute_distances() {
    DEBUG_MAS(std::cout << tag() << std::flush;
              );
    if (are_distances_computed()) {
        DEBUG_MAS(std::cout << "distances already known" << std::endl;
                  );
        return;
    }

    assert(init_distances.empty() && goal_distances.empty());

    if (init_state == PRUNED_STATE) {
        std::cout << "init state was pruned, no distances to compute" << std::endl;
        // If init_state was pruned, then everything must have been pruned.
        assert(num_states == 0);
        max_f = max_g = max_h = PLUS_INFINITY;
        return;
    }

    init_distances.resize(num_states, PLUS_INFINITY);
    goal_distances.resize(num_states, PLUS_INFINITY);
    if (labels->is_unit_cost()) {
        DEBUG_MAS(std::cout << "computing distances using unit-cost algorithm" << std::endl;
                  );
        compute_init_distances_unit_cost();
        compute_goal_distances_unit_cost();
    } else {
        DEBUG_MAS(std::cout << "computing distances using general-cost algorithm" << std::endl;
                  );
        compute_init_distances_general_cost();
        compute_goal_distances_general_cost();
    }

    max_f = 0;
    max_g = 0;
    max_h = 0;

    int unreachable_count = 0, irrelevant_count = 0;
    for (int i = 0; i < num_states; i++) {
        int g = init_distances[i];
        int h = goal_distances[i];
        // States that are both unreachable and irrelevant are counted
        // as unreachable, not irrelevant. (Doesn't really matter, of
        // course.)
        if (g == PLUS_INFINITY) {
            unreachable_count++;
        } else if (h == PLUS_INFINITY) {
            irrelevant_count++;
        } else {
            max_f = std::max(max_f, g + h);
            max_g = std::max(max_g, g);
            max_h = std::max(max_h, h);
        }
    }
    if (unreachable_count || irrelevant_count) {
        std::cout << tag()
                  << "unreachable: " << unreachable_count << " states, "
                  << "irrelevant: " << irrelevant_count << " states" << std::endl;
        /* Call shrink to discard unreachable and irrelevant states.
           The strategy must be one that prunes unreachable/irrelevant
           notes, but beyond that the details don't matter, as there
           is no need to actually shrink. So faster methods should be
           preferred. */

        /* TODO: Create a dedicated shrinking strategy from scratch,
           e.g. a bucket-based one that simply generates one good and
           one bad bucket? */

        // TODO/HACK: The way this is created is of course unspeakably
        // ugly. We'll leave this as is for now because there will likely
        // be more structural changes soon.
        ShrinkStrategy *shrink_temp = ShrinkFH::create_default(num_states);
        shrink_temp->shrink(*this, num_states, true);
        delete shrink_temp;
    }
}

static void breadth_first_search(
    const std::vector<std::vector<int>> &graph, std::deque<int> &queue,
    std::vector<int> &distances) {
    while (!queue.empty()) {
        int state = queue.front();
        queue.pop_front();
        for (int i = 0; i < graph[state].size(); i++) {
            int successor = graph[state][i];
            if (distances[successor] > distances[state] + 1) {
                distances[successor] = distances[state] + 1;
                queue.push_back(successor);
            }
        }
    }
}

void Abstraction::compute_init_distances_unit_cost() {
    std::vector<std::vector<AbstractStateRef>> forward_graph(num_states);
    for (int label_no = 0; label_no < num_labels; label_no++) {
        const std::vector<AbstractTransition> &transitions = transitions_by_label[label_no];
        for (const auto &trans: transitions) {
            forward_graph[trans.src].push_back(trans.target);
        }
    }

    std::deque<AbstractStateRef> queue;
    for (AbstractStateRef state = 0; state < num_states; state++) {
        if (state == init_state) {
            init_distances[state] = 0;
            queue.push_back(state);
        }
    }
    breadth_first_search(forward_graph, queue, init_distances);
}

void Abstraction::compute_goal_distances_unit_cost() {
    std::vector<std::vector<AbstractStateRef>> backward_graph(num_states);
    for (int label_no = 0; label_no < num_labels; label_no++) {
        const std::vector<AbstractTransition> &transitions = transitions_by_label[label_no];
        for (const auto &trans: transitions) {
            backward_graph[trans.target].push_back(trans.src);
        }
    }

    std::deque<AbstractStateRef> queue;
    for (AbstractStateRef state = 0; state < num_states; state++) {
        if (goal_states[state]) {
            goal_distances[state] = 0;
            queue.push_back(state);
        }
    }
    breadth_first_search(backward_graph, queue, goal_distances);
}

static void dijkstra_search(
    const std::vector<std::vector<std::pair<int, int>>> &graph,
    AdaptiveQueue<int, int> &queue,
    std::vector<int> &distances) {
    while (!queue.empty()) {
        std::pair<int, int> top_pair = queue.pop();
        int distance = top_pair.first;
        int state = top_pair.second;
        int state_distance = distances[state];
        assert(state_distance <= distance);
        if (state_distance < distance) {
            continue;
        }
        for (const auto &transition: graph[state]) {
            int successor = transition.first;
            int cost = transition.second;
            int successor_cost = state_distance + cost;
            if (distances[successor] > successor_cost) {
                distances[successor] = successor_cost;
                queue.push(successor_cost, successor);
            }
        }
    }
}

void Abstraction::compute_init_distances_general_cost() {
    std::vector<std::vector<std::pair<int, int>>> forward_graph(num_states);
    for (int label_no = 0; label_no < num_labels; label_no++) {
        int label_cost = get_label_cost_by_index(label_no);
        const std::vector<AbstractTransition> &transitions = transitions_by_label[label_no];
        for (const auto &trans: transitions) {
            forward_graph[trans.src].push_back(std::make_pair(trans.target, label_cost));
        }
    }

    // TODO: Reuse the same queue for multiple computations to save speed?
    //       Also see compute_goal_distances_general_cost.
    AdaptiveQueue<int, int> queue;
    for (AbstractStateRef state = 0; state < num_states; state++) {
        if (state == init_state) {
            init_distances[state] = 0;
            queue.push(0, state);
        }
    }
    dijkstra_search(forward_graph, queue, init_distances);
}

void Abstraction::compute_goal_distances_general_cost() {
    std::vector<std::vector<std::pair<int, int>>> backward_graph(num_states);
    for (int label_no = 0; label_no < num_labels; label_no++) {
        int label_cost = get_label_cost_by_index(label_no);
        const std::vector<AbstractTransition> &transitions = transitions_by_label[label_no];
        for (const auto &trans: transitions) {
            backward_graph[trans.target].push_back(std::make_pair(trans.src, label_cost));
        }
    }

    // TODO: Reuse the same queue for multiple computations to save speed?
    //       Also see compute_init_distances_general_cost.
    AdaptiveQueue<int, int> queue;
    for (AbstractStateRef state = 0; state < num_states; state++) {
        if (goal_states[state]) {
            goal_distances[state] = 0;
            queue.push(0, state);
        }
    }
    dijkstra_search(backward_graph, queue, goal_distances);
}

std::vector<IntEpsilonSum> Abstraction::recompute_goal_distances_with_epsilon() const {
    std::vector<IntEpsilonSum> new_goal_distances(num_states, std::numeric_limits<int>::max());
    std::vector<std::vector<std::pair<int, IntEpsilonSum>>> backward_graph(num_states);
    for (int label_no = 0; label_no < num_labels; label_no++) {
        auto label_cost = epsilon_if_zero<IntEpsilonSum>(get_label_cost_by_index(label_no));
        assert(label_cost != IntEpsilonSum(0));

        const std::vector<AbstractTransition> &transitions = transitions_by_label[label_no];
        for (const auto &trans: transitions) {
            backward_graph[trans.target].push_back(std::make_pair(trans.src, label_cost));
        }
    }

    // TODO: Reuse the same queue for multiple computations to save speed?
    //       Also see compute_init_distances_general_cost.
    HeapQueue<IntEpsilonSum, int> queue;
    for (AbstractStateRef state = 0; state < num_states; state++) {
        if (goal_states[state]) {
            new_goal_distances[state] = 0;
            queue.push(0, state);
        }
    }
    dijkstra_search_epsilon(backward_graph, queue, new_goal_distances, nullptr);
    return new_goal_distances;
}


void AtomicAbstraction::apply_abstraction_to_lookup_table(
    const std::vector<AbstractStateRef> &abstraction_mapping) {
    DEBUG_MAS(std::cout << tag() << "applying abstraction to lookup table" << std::endl;
              );
    for (int &table_entry: lookup_table) {
        AbstractStateRef old_state = table_entry;
        if (old_state != PRUNED_STATE)
            table_entry = abstraction_mapping[old_state];
    }
    if (simulation_relation) {
        simulation_relation->apply_shrinking_to_table(abstraction_mapping);
    }
}

void CompositeAbstraction::apply_abstraction_to_lookup_table(
    const std::vector<AbstractStateRef> &abstraction_mapping) {
    DEBUG_MAS(std::cout << tag() << "applying abstraction to lookup table" << std::endl;
              );
    for (int i = 0; i < components[0]->size(); i++) {
        for (int j = 0; j < components[1]->size(); j++) {
            AbstractStateRef old_state = lookup_table[i][j];
            if (old_state != PRUNED_STATE)
                lookup_table[i][j] = abstraction_mapping[old_state];
        }
    }
    if (simulation_relation) {
        simulation_relation->apply_shrinking_to_table(abstraction_mapping);
    }
}

bool Abstraction::are_transitions_sorted_unique() const {
    for (const auto &transitions: transitions_by_label) {
        if (!is_sorted_unique(transitions)) {
            return false;
        }
    }
    return true;
}

bool Abstraction::is_normalized() const {
    return (num_labels == labels->get_size()) && transitions_sorted_unique;
}

void Abstraction::normalize() {
    /* This method sorts all transitions and removes duplicate transitions.
       It also maps the labels so that they are up to date with the labels
       object. */

    if (is_normalized()) {
        return;
    }
    if (store_original_operators) {
        normalize2();
        return;
    }
    lts.reset();     //In future versions, normalize the LTS instead
    //cout << tag() << "normalizing" << std::endl;

    typedef std::vector<std::pair<AbstractStateRef, int>> StateBucket;

    /* First, partition by target state. Possibly replace labels by
       their new label which they are mapped to via label reduction and clear
       away the transitions that have been processed. */
    std::vector<StateBucket> target_buckets(num_states);

    /* We handle "old" and "new" labels separately. The following loop
       goes over all labels that were already known when we last
       normalized. It ignores labels that are no longer current
       because they have been mapped to fresh labels in the last label
       reduction. (These labels are handled separately in the next
       loop.) */

    for (int label_no = 0; label_no < num_labels; label_no++) {
        if (labels->is_label_reduced(label_no)) {
            // skip all labels that have been reduced (previously, in which case
            // they do not induce any transitions or recently, in which case we
            // deal with them separately).
            continue;
        }
        std::vector<AbstractTransition> &transitions = transitions_by_label[label_no];
        for (const auto &t: transitions) {
            target_buckets[t.target].push_back(std::make_pair(t.src, label_no));
        }
        std::vector<AbstractTransition>().swap(transitions);
    }

    /* Now we handle "new" labels. We iterate over the fresh labels and
       determine the "old" labels from which they were generated to combine
       their transitions.

       Some complications arise when we combine labels of which some
       are relevant and others are not. In this case, we test if all
       transitions of relevant labels are self-loops. If this happens,
       we make the new label irrelevant. Otherwise, the new labels is
       relevant and we must materialize all previously implicit
       self-loops.

       Note that currently we do not detect the case where a label
       becomes irrelevant due to shrinking. This could be a future
       optimization.
    */

    /* labels_made_irrelevant stores labels for which we collect
       transitions that later turn out to be unnecessary because the
       label becomes irrelevant.
    */
    std::unordered_set<int> labels_made_irrelevant;
    for (int reduced_label_no = num_labels; reduced_label_no < labels->get_size();
         ++reduced_label_no) {
        const Label *reduced_label = labels->get_label_by_index(reduced_label_no);
        const std::vector<Label *> &parents = reduced_label->get_parents();
        bool some_parent_is_irrelevant = false;
        bool all_transitions_are_self_loops = true;
        for (auto parent: parents) {
            int parent_id = parent->get_id();
            // We require that we only have to deal with one label reduction at
            // a time when normalizing. Otherwise, the following assertion could
            // be broken, and we would need to consider parents' parents and so on...
            assert(parent_id < num_labels);
            std::vector<AbstractTransition> &transitions = transitions_by_label[parent_id];

            if (relevant_labels[parent_id]) {
                for (const auto &t: transitions) {
                    target_buckets[t.target].push_back(std::make_pair(t.src, reduced_label_no));
                    if (t.target != t.src) {
                        all_transitions_are_self_loops = false;
                    }
                }
                std::vector<AbstractTransition>().swap(transitions);

                // mark parent as irrelevant (unused labels should not be
                // marked as relevant in order to avoid confusions when
                // considering all relevant labels).
                relevant_labels[parent_id] = false;
                labels->set_irrelevant_for(parent_id, this);
            } else {
                some_parent_is_irrelevant = true;
            }
        }
        if (some_parent_is_irrelevant) {
            if (all_transitions_are_self_loops) {
                // new label is irrelevant (implicit self-loops)
                // remove all transitions (later)
                labels_made_irrelevant.insert(reduced_label_no);
                labels->set_irrelevant_for(reduced_label_no, this);
            } else {
                // new label is relevant
                relevant_labels[reduced_label_no] = true;
                labels->set_relevant_for(reduced_label_no, this);
                // make self loops explicit
                for (int i = 0; i < num_states; ++i) {
                    target_buckets[i].push_back(std::make_pair(i, reduced_label_no));
                }
            }
        } else {
            // new label is relevant
            relevant_labels[reduced_label_no] = true;
            labels->set_relevant_for(reduced_label_no, this);
        }
    }

    // Second, partition by src state.
    std::vector<StateBucket> src_buckets(num_states);

    for (AbstractStateRef target = 0; target < num_states; target++) {
        StateBucket &bucket = target_buckets[target];
        for (auto &item: bucket) {
            AbstractStateRef src = item.first;
            int label_no = item.second;
            if (labels_made_irrelevant.count(label_no)) {
                assert(transitions_by_label[label_no].empty());
            } else {
                src_buckets[src].push_back(std::make_pair(target, label_no));
            }
        }
    }
    std::vector<StateBucket>().swap(target_buckets);

    // Finally, partition by operator and drop duplicates.
    for (AbstractStateRef src = 0; src < num_states; src++) {
        StateBucket &bucket = src_buckets[src];
        for (auto &item: bucket) {
            int target = item.first;
            int label_no = item.second;

            std::vector<AbstractTransition> &op_bucket = transitions_by_label[label_no];
            AbstractTransition trans(src, target);
            if (op_bucket.empty() || op_bucket.back() != trans)
                op_bucket.push_back(trans);
        }
    }

    // Abstraction has been normalized, restore invariant
    assert(are_transitions_sorted_unique());
    num_labels = labels->get_size();
    transitions_sorted_unique = true;
    assert(is_normalized());
}

void Abstraction::normalize2() {
    /* This method sorts all transitions and removes duplicate transitions.
       It also maps the labels so that they are up to date with the labels
       object. */

    lts.reset();     //In future versions, normalize the LTS instead
    //cout << tag() << "normalizing" << std::endl;

    typedef std::vector<std::pair<AbstractStateRef, std::pair<int, boost::dynamic_bitset<>>>> StateBucket;

    /* First, partition by target state. Possibly replace labels by
       their new label which they are mapped to via label reduction and clear
       away the transitions that have been processed. */
    std::vector<StateBucket> target_buckets(num_states);

    /* We handle "old" and "new" labels separately. The following loop
       goes over all labels that were already known when we last
       normalized. It ignores labels that are no longer current
       because they have been mapped to fresh labels in the last label
       reduction. (These labels are handled separately in the next
       loop.) */

    for (int label_no = 0; label_no < num_labels; label_no++) {
        if (labels->is_label_reduced(label_no)) {
            // skip all labels that have been reduced (previously, in which case
            // they do not induce any transitions or recently, in which case we
            // deal with them separately).
            continue;
        }
        std::vector<AbstractTransition> &transitions = transitions_by_label[label_no];
        for (int i = 0; i < transitions.size(); i++) {
            const AbstractTransition &t = transitions[i];
            target_buckets[t.target].push_back(
                make_pair(t.src, make_pair(label_no,
                                           transitions_by_label_based_on_operators[label_no][i])));
        }
        std::vector<AbstractTransition>().swap(transitions);
        std::vector<boost::dynamic_bitset<>>().swap(transitions_by_label_based_on_operators[label_no]);
    }

    /* Now we handle "new" labels. We iterate over the fresh labels and
       determine the "old" labels from which they were generated to combine
       their transitions.

       Some complications arise when we combine labels of which some
       are relevant and others are not. In this case, we test if all
       transitions of relevant labels are self-loops. If this happens,
       we make the new label irrelevant. Otherwise, the new labels is
       relevant and we must materialize all previously implicit
       self-loops.

       Note that currently we do not detect the case where a label
       becomes irrelevant due to shrinking. This could be a future
       optimization.
    */

    /* labels_made_irrelevant stores labels for which we collect
       transitions that later turn out to be unnecessary because the
       label becomes irrelevant.
    */
    std::unordered_set<int> labels_made_irrelevant;
    for (int reduced_label_no = num_labels; reduced_label_no < labels->get_size();
         ++reduced_label_no) {
        const Label *reduced_label = labels->get_label_by_index(reduced_label_no);
        const std::vector<Label *> &parents = reduced_label->get_parents();
        bool some_parent_is_irrelevant = false;
        bool all_transitions_are_self_loops = true;
        for (auto parent: parents) {
            int parent_id = parent->get_id();
            // We require that we only have to deal with one label reduction at
            // a time when normalizing. Otherwise. the following assertion could
            // be broken, and we would need to consider parents' parents and so on...
            assert(parent_id < num_labels);
            std::vector<AbstractTransition> &transitions =
                transitions_by_label[parent_id];

            if (relevant_labels[parent_id]) {
                for (int j = 0; j < transitions.size(); j++) {
                    const AbstractTransition &t = transitions[j];
                    target_buckets[t.target].push_back(
                        make_pair(t.src, make_pair(reduced_label_no,
                                                   transitions_by_label_based_on_operators[parent_id][j])));
                    if (t.target != t.src) {
                        all_transitions_are_self_loops = false;
                    }
                }
                std::vector<AbstractTransition>().swap(transitions);
                std::vector<boost::dynamic_bitset<>>().swap(transitions_by_label_based_on_operators[parent_id]);

                // mark parent as irrelevant (unused labels should not be
                // marked as relevant in order to avoid confusions when
                // considering all relevant labels).
                relevant_labels[parent_id] = false;
                labels->set_irrelevant_for(parent_id, this);
            } else {
                some_parent_is_irrelevant = true;
            }
        }
        if (some_parent_is_irrelevant) {
            if (all_transitions_are_self_loops) {
                // new label is irrelevant (implicit self-loops)
                // remove all transitions (later)
                labels_made_irrelevant.insert(reduced_label_no);
                labels->set_irrelevant_for(reduced_label_no, this);
            } else {
                // new label is relevant
                relevant_labels[reduced_label_no] = true;
                labels->set_relevant_for(reduced_label_no, this);
                // make self loops explicit
                for (int i = 0; i < num_states; ++i) {
                    boost::dynamic_bitset<> original_operators(global_simulation_task()->get_num_operators(),
                                                               false);
                    const Label *label = labels->get_label_by_index(reduced_label_no);
                    std::set<int> op_ids;
                    label->get_operators(op_ids);
                    for (auto id: op_ids) {
                        original_operators[id] = true;
                    }
                    target_buckets[i].push_back(
                        make_pair(i, make_pair(reduced_label_no, original_operators)));
                }
            }
        } else {
            // new label is relevant
            relevant_labels[reduced_label_no] = true;
            labels->set_relevant_for(reduced_label_no, this);
        }
    }

    // Second, partition by src state.
    std::vector<StateBucket> src_buckets(num_states);

    for (AbstractStateRef target = 0; target < num_states; target++) {
        StateBucket &bucket = target_buckets[target];
        for (auto &item: bucket) {
            AbstractStateRef src = item.first;
            int label_no = item.second.first;
            if (labels_made_irrelevant.count(label_no)) {
                assert(transitions_by_label[label_no].empty());
            } else {
                src_buckets[src].push_back(make_pair(target, make_pair(label_no, item.second.second)));
            }
        }
    }
    std::vector<StateBucket>().swap(target_buckets);

    // Finally, partition by operator and drop duplicates.
    for (AbstractStateRef src = 0; src < num_states; src++) {
        StateBucket &bucket = src_buckets[src];
        for (auto &item: bucket) {
            int target = item.first;
            int label_no = item.second.first;

            std::vector<AbstractTransition> &op_bucket = transitions_by_label[label_no];
            AbstractTransition trans(src, target);
            if (op_bucket.empty() || op_bucket.back() != trans) {
                transitions_by_label_based_on_operators[label_no].push_back(item.second.second);
                op_bucket.push_back(trans);
            } else {
                transitions_by_label_based_on_operators[label_no].back() |= item.second.second;
                //op_bucket.back().based_on_operators |= bucket[i].second.second;
            }
        }
    }

    // Abstraction has been normalized, restore invariant
    assert(are_transitions_sorted_unique());
    num_labels = labels->get_size();
    transitions_sorted_unique = true;
    assert(is_normalized());
}

EquivalenceRelation *Abstraction::compute_local_equivalence_relation() const {
    /* If label l1 is irrelevant and label l2 is relevant but has exactly the
       transitions of an irrelevant label, we do not detect the equivalence. */

    assert(is_normalized());
    std::vector<bool> considered_labels(num_labels, false);
    std::vector<std::pair<int, int>> annotated_labels;
    int annotation = 0;
    for (int label_no = 0; label_no < num_labels; ++label_no) {
        if (labels->is_label_reduced(label_no)) {
            // do not consider non-leaf labels
            continue;
        }
        if (considered_labels[label_no]) {
            continue;
        }
        int label_cost = get_label_cost_by_index(label_no);
        annotated_labels.emplace_back(annotation, label_no);
        const std::vector<AbstractTransition> &transitions = transitions_by_label[label_no];
        for (int other_label_no = label_no + 1; other_label_no < num_labels; ++other_label_no) {
            if (labels->is_label_reduced(other_label_no)) {
                // do not consider non-leaf labels
                continue;
            }
            if (considered_labels[other_label_no]) {
                continue;
            }
            if (label_cost != get_label_cost_by_index(other_label_no)) {
                continue;
            }
            if (relevant_labels[label_no] != relevant_labels[other_label_no]) {
                continue;
            }
            const std::vector<AbstractTransition> &other_transitions = transitions_by_label[other_label_no];
            if ((transitions.empty() && other_transitions.empty())
                || (transitions == other_transitions)) {
                considered_labels[other_label_no] = true;
                annotated_labels.emplace_back(annotation, other_label_no);
            }
        }
        ++annotation;
    }
    return EquivalenceRelation::from_annotated_elements<int>(num_labels, annotated_labels);
}

void Abstraction::build_atomic_abstractions(std::vector<Abstraction *> &result,
                                            Labels *labels) {
    assert(result.empty());
    std::cout << "Building atomic abstractions... " << std::endl;
    int var_count = global_simulation_task()->get_num_variables();

    // Step 1: Create the abstraction objects without transitions.
    for (int var_no = 0; var_no < var_count; var_no++)
        result.push_back(new AtomicAbstraction(labels, var_no));

    // Step 2: Add transitions.
    // Note that when building atomic abstractions, no other labels than the
    // original operators have been added yet.
    for (int label_no = 0; label_no < labels->get_size(); label_no++) {
        if (is_dead(label_no))
            continue;
        const Label *label = labels->get_label_by_index(label_no);
        const std::vector<Prevail> &prevs = label->get_prevail();
        for (const auto &prev: prevs) {
            int var = prev.var;
            int value = prev.prev;
            Abstraction *abs = result[var];
            AbstractTransition trans(value, value);
            if (store_original_operators) {
                /* PIET-edit: We should at some point change here from the full size to the subset corresponding
                 * only to the operators as relevant for this label.
                 */
                boost::dynamic_bitset<> empty_bitset(global_simulation_task()->get_num_operators());
                empty_bitset[label_no] = true;
                abs->transitions_by_label_based_on_operators[label_no].push_back(empty_bitset);
            }

            abs->transitions_by_label[label_no].push_back(trans);
            abs->relevant_labels[label_no] = true;
            labels->set_relevant_for(label_no, abs);
        }
        const std::vector<PrePost> &pre_posts = label->get_pre_post();
        for (const auto &pre_post: pre_posts) {
            int var = pre_post.var;
            int post_value = pre_post.post;
            Abstraction *abs = result[var];

            // Determine possible values that var can have when this
            // operator is applicable.
            int pre_value = pre_post.pre;
            int pre_value_min, pre_value_max;
            if (pre_value == -1) {
                pre_value_min = 0;
                pre_value_max = global_simulation_task()->get_variable_domain_size(var);
            } else {
                pre_value_min = pre_value;
                pre_value_max = pre_value + 1;
            }

            // cond_effect_pre_value == x means that the effect has an
            // effect condition "var == x".
            // cond_effect_pre_value == -1 means no effect condition on var.
            // has_other_effect_cond is true iff there exists an effect
            // condition on a variable other than var.
            const std::vector<Prevail> &eff_conds = pre_post.cond;
            int cond_effect_pre_value = -1;
            bool has_other_effect_cond = false;
            for (const auto &eff_cond: eff_conds) {
                if (eff_cond.var == var) {
                    cond_effect_pre_value = eff_cond.prev;
                } else {
                    has_other_effect_cond = true;
                }
            }

            // Handle transitions that occur when the effect triggers.
            for (int value = pre_value_min; value < pre_value_max; value++) {
                /* Only add a transition if it is possible that the effect
                   triggers. We can rule out that the effect triggers if it has
                   a condition on var and this condition is not satisfied. */
                if (cond_effect_pre_value == -1 || cond_effect_pre_value == value) {
                    AbstractTransition trans(value, post_value);
                    if (store_original_operators) {
                        /* PIET-edit: We should at some point change here from the full size to the subset corresponding
                         * only to the operators as relevant for this label.
                         */
                        boost::dynamic_bitset<> empty_bitset(global_simulation_task()->get_num_operators());
                        empty_bitset[label_no] = true;
                        abs->transitions_by_label_based_on_operators[label_no].push_back(empty_bitset);
                    }
                    abs->transitions_by_label[label_no].push_back(trans);
                }
            }

            // Handle transitions that occur when the effect does not trigger.
            if (!eff_conds.empty()) {
                for (int value = pre_value_min; value < pre_value_max; value++) {
                    /* Add self-loop if the effect might not trigger.
                       If the effect has a condition on another variable, then
                       it can fail to trigger no matter which value var has.
                       If it only has a condition on var, then the effect
                       fails to trigger if this condition is false. */
                    if (has_other_effect_cond || value != cond_effect_pre_value) {
                        AbstractTransition loop(value, value);
                        if (store_original_operators) {
                            /* PIET-edit: We should at some point change here from the full size to the subset corresponding
                             * only to the operators as relevant for this label.
                             */
                            boost::dynamic_bitset<> empty_bitset(global_simulation_task()->get_num_operators());
                            empty_bitset[label_no] = true;
                            abs->transitions_by_label_based_on_operators[label_no].push_back(empty_bitset);
                        }
                        abs->transitions_by_label[label_no].push_back(loop);
                    }
                }
            }

            abs->relevant_labels[label_no] = true;
            labels->set_relevant_for(label_no, abs);
        }
    }

#ifndef NDEBUG
    for (auto &i: result) {
        assert(i->are_transitions_sorted_unique());
        assert(i->is_normalized());
    }
#endif
}

AtomicAbstraction::AtomicAbstraction(Labels *labels, int variable_)
    : Abstraction(labels), variable(variable_) {
    varset.push_back(variable);
    /*
      This generates the states of the atomic abstraction, but not the
      arcs: It is more efficient to generate all arcs of all atomic
      abstractions simultaneously.
    */
    int range = global_simulation_task()->get_variable_domain_size(variable);

    int init_value = global_simulation_task()->get_initial_state_values()[variable];
    int goal_value = -1;
    goal_relevant_vars = 0;
    const int goal_size = global_simulation_task()->get_num_goals();
    for (int goal_no = 0; goal_no < goal_size; goal_no++) {
        if (global_simulation_task()->get_goal_fact(goal_no).var == variable) {
            goal_relevant_vars++;
            assert(goal_value == -1);
            goal_value = global_simulation_task()->get_goal_fact(goal_no).value;
        }
    }
    all_goals_relevant = goal_relevant_vars == goal_size;

    num_states = range;
    lookup_table.reserve(range);
    goal_states.resize(num_states, false);
    for (int value = 0; value < range; value++) {
        if (value == goal_value || goal_value == -1) {
            goal_states[value] = true;
        }
        if (value == init_value)
            init_state = value;
        lookup_table.push_back(value);
    }
}

CompositeAbstraction::CompositeAbstraction(Labels *labels, Abstraction *abs1, Abstraction *abs2)
    : Abstraction(labels) {
    DEBUG_MAS(std::cout << "Merging " << abs1->description() << " and " << abs2->description() << std::endl;
              );

    assert(abs1->is_solvable() && abs2->is_solvable());
    assert(abs1->is_normalized() && abs2->is_normalized());

    components[0] = abs1;
    components[1] = abs2;

    std::set_union(abs1->varset.begin(), abs1->varset.end(), abs2->varset.begin(),
                   abs2->varset.end(), back_inserter(varset));

    num_states = abs1->size() * abs2->size();
    goal_states.resize(num_states, false);
    goal_relevant_vars = (abs1->goal_relevant_vars + abs2->goal_relevant_vars);
    all_goals_relevant = goal_relevant_vars == global_simulation_task()->get_num_goals();
    lookup_table.resize(abs1->size(), std::vector<AbstractStateRef>(abs2->size()));
    for (int s1 = 0; s1 < abs1->size(); s1++) {
        for (int s2 = 0; s2 < abs2->size(); s2++) {
            int state = s1 * abs2->size() + s2;
            lookup_table[s1][s2] = state;
            if (abs1->goal_states[s1] && abs2->goal_states[s2])
                goal_states[state] = true;
            if (s1 == abs1->init_state && s2 == abs2->init_state)
                init_state = state;
        }
    }

    /* Note:
       The way we construct the transitions of the new composite abstraction,
       we cannot easily guarantee that they are sorted. Given that we have
       transitions a->b and c->d in abstraction one and two, we would like to
       have (a,c)->(b,d) in the resulting abstraction. There is no obvious way
       at looking at the transitions of abstractions one and two that would
       result in the desired ordering. Even in the case that the second
       abstractions has only self loops, this is not trivial, as we would like
       to sort transitions to (a,c,b). Only in the case that the first
       abstraction has only self-lopos, by looking at each transition of the
       first abstraction and multiplying in out with the transitions of the
       second transition, we obtain the desired order (a,c,d).
    */
    int multiplier = abs2->size();
    for (int label_no = 0; label_no < num_labels; label_no++) {
        bool relevant1 = abs1->relevant_labels[label_no];
        bool relevant2 = abs2->relevant_labels[label_no];
        if (relevant1 || relevant2) {
            relevant_labels[label_no] = true;
            labels->set_relevant_for(label_no, this);
            std::vector<AbstractTransition> &transitions = transitions_by_label[label_no];
            const std::vector<AbstractTransition> &bucket1 =
                abs1->transitions_by_label[label_no];
            const std::vector<AbstractTransition> &bucket2 =
                abs2->transitions_by_label[label_no];
            if (relevant1 && relevant2) {
                transitions.reserve(bucket1.size() * bucket2.size());
                for (int i = 0; i < bucket1.size(); i++) {
                    int src1 = bucket1[i].src;
                    int target1 = bucket1[i].target;
                    for (int j = 0; j < bucket2.size(); j++) {
                        int src2 = bucket2[j].src;
                        int target2 = bucket2[j].target;
                        int src = src1 * multiplier + src2;
                        int target = target1 * multiplier + target2;

                        transitions.emplace_back(src, target);
                        if (store_original_operators) {
                            /* PIET-edit: We should at some point change here from the full size to the subset corresponding
                             * only to the operators as relevant for this label.
                             */

                            transitions_by_label_based_on_operators[label_no].push_back(
                                abs1->transitions_by_label_based_on_operators[label_no][i] &
                                abs2->transitions_by_label_based_on_operators[label_no][j]);
                            assert(transitions_by_label_based_on_operators[label_no].back().count());
                        }
                    }
                }
            } else if (relevant1) {
                assert(!relevant2);
                transitions.reserve(bucket1.size() * abs2->size());
                for (int i = 0; i < bucket1.size(); i++) {
                    int src1 = bucket1[i].src;
                    int target1 = bucket1[i].target;
                    for (int s2 = 0; s2 < abs2->size(); s2++) {
                        int src = src1 * multiplier + s2;
                        int target = target1 * multiplier + s2;
                        if (store_original_operators) {
                            /* PIET-edit: We should at some point change here from the full size to the subset corresponding
                             * only to the operators as relevant for this label.
                             */

                            transitions_by_label_based_on_operators[label_no].push_back(
                                abs1->transitions_by_label_based_on_operators[label_no][i]);
                        }
                        transitions.emplace_back(src, target);
                    }
                }
            } else if (relevant2) {
                assert(!relevant1);
                transitions.reserve(bucket2.size() * abs1->size());
                for (int s1 = 0; s1 < abs1->size(); s1++) {
                    for (int i = 0; i < bucket2.size(); i++) {
                        int src2 = bucket2[i].src;
                        int target2 = bucket2[i].target;
                        int src = s1 * multiplier + src2;
                        int target = s1 * multiplier + target2;


                        if (store_original_operators) {
                            /* PIET-edit: We should at some point change here from the full size to the subset corresponding
                             * only to the operators as relevant for this label.
                             */
                            transitions_by_label_based_on_operators[label_no].push_back(
                                abs2->transitions_by_label_based_on_operators[label_no][i]);
                        }

                        transitions.emplace_back(src, target);
                    }
                }
                assert(is_sorted_unique(transitions));
            }
        }
    }

    // TODO do not check if transitions are sorted but just assume they are not?
    if (!are_transitions_sorted_unique())
        transitions_sorted_unique = false;

    labels->set_irrelevant_for_all_labels(abs1);
    labels->set_irrelevant_for_all_labels(abs2);
}

std::string AtomicAbstraction::description() const {
    std::ostringstream s;
    s << "atomic abstraction #" << variable;
    return s.str();
}

std::string AtomicAbstraction::description(int s) const {
    std::stringstream ss;
    ss << "(";
    for (int i = 0; i < lookup_table.size(); i++) {
        if (s == lookup_table[i]) {
            ss << global_simulation_task()->get_fact_name(FactPair(variable, i));
        }
    }
    ss << ")";
    return ss.str();
}

std::string CompositeAbstraction::description() const {
    std::ostringstream s;
    s << "abstraction (" << varset.size() << "/"
      << global_simulation_task()->get_num_variables() << " vars)";
    return s.str();
}

std::string CompositeAbstraction::description(int s) const {
    std::stringstream ss;
    ss << "s" << s;
    return ss.str();
}

AbstractStateRef AtomicAbstraction::get_abstract_state(const State &state) const {
    int value = state[variable].get_value();
    return lookup_table[value];
}

AbstractStateRef AtomicAbstraction::get_abstract_state(const std::vector<int> &state) const {
    int value = state[variable];
    return lookup_table[value];
}

AbstractStateRef AtomicAbstraction::get_atomic_abstract_state(int local_state_value) const {
    return lookup_table[local_state_value];
}

AbstractStateRef CompositeAbstraction::get_abstract_state(const State &state) const {
    AbstractStateRef state1 = components[0]->get_abstract_state(state);
    AbstractStateRef state2 = components[1]->get_abstract_state(state);
    if (state1 == PRUNED_STATE || state2 == PRUNED_STATE)
        return PRUNED_STATE;
    return lookup_table[state1][state2];
}


AbstractStateRef CompositeAbstraction::get_abstract_state(const std::vector<int> &state) const {
    AbstractStateRef state1 = components[0]->get_abstract_state(state);
    AbstractStateRef state2 = components[1]->get_abstract_state(state);
    if (state1 == PRUNED_STATE || state2 == PRUNED_STATE)
        return PRUNED_STATE;
    return lookup_table[state1][state2];
}

void Abstraction::apply_abstraction(
    std::vector<__gnu_cxx::slist<AbstractStateRef>> &collapsed_groups) {
    /* Note on how this method interacts with the distance information
       (init_distances and goal_distances): if no two states with
       different g or h values are combined by the abstraction (i.e.,
       if the abstraction is "f-preserving", then this method makes
       sure that distance information is preserved.

       This is important because one of the (indirect) callers of this
       method is the distance computation code, which uses it in a
       somewhat roundabout way to get rid of irrelevant and
       unreachable states. That caller will always give us an
       f-preserving abstraction.

       When called with a non-f-preserving abstraction, distance
       information is cleared as a side effect. In most cases we won't
       actually need it anymore at this point anyway, so it is no
       great loss.

       Still, it might be good if we could find a way to perform the
       unreachability and relevance pruning that didn't introduce such
       tight coupling between the distance computation and abstraction
       code. It would probably also a good idea to do the
       unreachability/relevance pruning as early as possible, e.g.
       right after construction.
    */

    // abstraction must have been normalized if any labels have been reduced
    // before. Note that we do *not* require transitions to be sorted (thus
    // not asserting is_normalized()), because shrink can indirectly be called
    // from the distance computation, which is done for the final abstraction
    // which on its turn may not be normalized at that time.
    //cout << "abs->num_labels: " << num_labels << " / labels->get_size: " << labels->get_size() << std::endl;
    assert(num_labels == labels->get_size());
    // distances must have been computed before
    //assert(are_distances_computed());
    //Alvaro: Not anymore! We handle the case were they have not been computed separately.

    // Don't apply abstractions if they are the identity function
    if (size() == collapsed_groups.size())
        return;


    DEBUG_MAS(std::cout << tag() << "applying abstraction (" << size()
                        << " to " << collapsed_groups.size() << " states)" << std::endl;
              );

    typedef __gnu_cxx::slist<AbstractStateRef> Group;

    std::vector<int> abstraction_mapping(num_states, PRUNED_STATE);

    for (int group_no = 0; group_no < collapsed_groups.size(); group_no++) {
        Group &group = collapsed_groups[group_no];
        for (int state: group) {
            assert(abstraction_mapping[state] == PRUNED_STATE);
            abstraction_mapping[state] = group_no;
        }
    }

    int new_num_states = collapsed_groups.size();
    std::vector<int> new_init_distances(new_num_states, PLUS_INFINITY);
    std::vector<int> new_goal_distances(new_num_states, PLUS_INFINITY);
    std::vector<bool> new_goal_states(new_num_states, false);

    bool must_clear_distances = false;
    if (!init_distances.empty()) {
        for (AbstractStateRef new_state = 0; new_state < collapsed_groups.size(); new_state++) {
            Group &group = collapsed_groups[new_state];
            assert(!group.empty());

            Group::iterator pos = group.begin();
            int &new_init_dist = new_init_distances[new_state];
            int &new_goal_dist = new_goal_distances[new_state];

            new_init_dist = init_distances[*pos];
            new_goal_dist = goal_distances[*pos];
            new_goal_states[new_state] = goal_states[*pos];

            ++pos;
            for (; pos != group.end(); ++pos) {
                if (init_distances[*pos] != new_init_dist) {
                    must_clear_distances = true;
                }
                if (goal_distances[*pos] != new_goal_dist) {
                    must_clear_distances = true;
                }
                if (goal_states[*pos])
                    new_goal_states[new_state] = true;
            }
        }

        // Release memory.
        std::vector<int>().swap(init_distances);
        std::vector<int>().swap(goal_distances);
        std::vector<bool>().swap(goal_states);
    } else {
        must_clear_distances = true;
        for (AbstractStateRef new_state = 0; new_state < collapsed_groups.size(); new_state++) {
            Group &group = collapsed_groups[new_state];
            assert(!group.empty());

            Group::iterator pos = group.begin();
            new_goal_states[new_state] = goal_states[*pos];

            ++pos;
            for (; pos != group.end(); ++pos) {
                if (goal_states[*pos])
                    new_goal_states[new_state] = true;
            }
        }

        // Release memory.
        std::vector<int>().swap(init_distances);
        std::vector<int>().swap(goal_distances);
        std::vector<bool>().swap(goal_states);
    }

    std::vector<std::vector<AbstractTransition>> new_transitions_by_label(
        transitions_by_label.size());

    std::vector<std::vector<boost::dynamic_bitset<>>> new_transitions_by_label_based_on_operators(
        transitions_by_label_based_on_operators.size());
    for (int label_no = 0; label_no < num_labels; label_no++) {
        if (labels->is_label_reduced(label_no)) {
            // do not consider non-leaf labels
            continue;
        }
        const std::vector<AbstractTransition> &transitions =
            transitions_by_label[label_no];
        std::vector<AbstractTransition> &new_transitions =
            new_transitions_by_label[label_no];
        new_transitions.reserve(transitions.size());
        for (int i = 0; i < transitions.size(); i++) {
            const AbstractTransition &trans = transitions[i];
            int src = abstraction_mapping[trans.src];
            int target = abstraction_mapping[trans.target];
            if (src != PRUNED_STATE && target != PRUNED_STATE) {
                new_transitions.emplace_back(src, target);
                if (store_original_operators) {
                    /* PIET-edit: We should at some point change here from the full size to the subset corresponding
                     * only to the operators as relevant for this label.
                     */
                    new_transitions_by_label_based_on_operators[label_no].push_back(
                        transitions_by_label_based_on_operators[label_no][i]);
                }
            }
        }
    }
    std::vector<std::vector<AbstractTransition>>().swap(transitions_by_label);

    if (store_original_operators) {
        transitions_by_label_based_on_operators.swap(new_transitions_by_label_based_on_operators);
    }

    num_states = new_num_states;
    transitions_by_label.swap(new_transitions_by_label);
    init_distances.swap(new_init_distances);
    goal_distances.swap(new_goal_distances);

    goal_states.swap(new_goal_states);
    init_state = abstraction_mapping[init_state];
    if (init_state == PRUNED_STATE) {
        std::cout << tag() << "initial state pruned; task unsolvable" << std::endl;
        exit_with(EXIT_UNSOLVABLE);
    }

    apply_abstraction_to_lookup_table(abstraction_mapping);

    if (must_clear_distances) {
        DEBUG_MAS(std::cout << tag() << "simplification was not f-preserving!" << std::endl;
                  );
        clear_distances();
    }

    // TODO do not check if transitions are sorted but just assume they are not?
    if (!are_transitions_sorted_unique())
        transitions_sorted_unique = false;
    lts.reset();
}

bool Abstraction::is_solvable() const {
    return init_state != PRUNED_STATE;
}

int Abstraction::get_cost(const State &state) const {
    int abs_state = get_abstract_state(state);
    if (abs_state == PRUNED_STATE)
        return -1;
    int cost = goal_distances[abs_state];
    assert(cost != PLUS_INFINITY);
    return cost;
}

unsigned int Abstraction::memory_estimate() const {
    unsigned int result = sizeof(Abstraction);
    result += sizeof(Label *) * relevant_labels.capacity();
    result += sizeof(std::vector<AbstractTransition>) * transitions_by_label.capacity();
    for (const auto &trans_vector: transitions_by_label)
        result += sizeof(AbstractTransition) * trans_vector.capacity();
    result += sizeof(int) * init_distances.capacity();
    result += sizeof(int) * goal_distances.capacity();
    result += sizeof(bool) * goal_states.capacity();
    return result;
}

unsigned int AtomicAbstraction::memory_estimate() const {
    unsigned int result = Abstraction::memory_estimate();
    result += sizeof(AtomicAbstraction) - sizeof(Abstraction);
    result += sizeof(AbstractStateRef) * lookup_table.capacity();
    return result;
}

unsigned int CompositeAbstraction::memory_estimate() const {
    unsigned int result = Abstraction::memory_estimate();
    result += sizeof(CompositeAbstraction) - sizeof(Abstraction);
    result += sizeof(std::vector<AbstractStateRef>) * lookup_table.capacity();
    for (const auto &item: lookup_table)
        result += sizeof(AbstractStateRef) * item.capacity();

    result += components[0]->memory_estimate() + components[1]->memory_estimate();
    return result;
}

void Abstraction::release_memory() {
    std::vector<bool>().swap(relevant_labels);
    std::vector<std::vector<AbstractTransition>>().swap(transitions_by_label);
    std::vector<std::vector<boost::dynamic_bitset<>>>().swap(transitions_by_label_based_on_operators);
    lts.reset();
}

int Abstraction::total_transitions() const {
    int total = 0;
    for (const auto &trans_vector: transitions_by_label)
        total += trans_vector.size();
    return total;
}

int Abstraction::total_transition_operators() const {
    int total = 0;
    for (const auto &transitions: transitions_by_label_based_on_operators) {
        for (const auto &trans: transitions) {
            total += trans.count();
        }
    }
    return total;
}

int Abstraction::unique_unlabeled_transitions() const {
    std::vector<AbstractTransition> unique_transitions;
    for (const auto &trans: transitions_by_label) {
        unique_transitions.insert(unique_transitions.end(), trans.begin(), trans.end());
    }
    std::sort(unique_transitions.begin(), unique_transitions.end());
    return unique(unique_transitions.begin(), unique_transitions.end())
           - unique_transitions.begin();
}

void Abstraction::statistics(bool include_expensive_statistics) const {
    unsigned int memory = memory_estimate();
    peak_memory = std::max(peak_memory, memory);
    std::cout << tag() << size() << " states, ";
    if (include_expensive_statistics)
        std::cout << unique_unlabeled_transitions() << "/";
    std::cout << total_transitions() << " arcs" << std::endl;

    if (store_original_operators && include_expensive_statistics) {
        std::cout << tag();
        std::cout << total_transition_operators() << " stored operators in transitions" << std::endl;
    }
    DEBUG_MSG(std::cout << tag();
              if (!are_distances_computed()) {
            std::cout << "distances not computed";
        } else if (is_solvable()) {
            std::cout << "init h=" << goal_distances[init_state] << ", max f=" << max_f
                      << ", max g=" << max_g << ", max h=" << max_h;
        } else {
            std::cout << "abstraction is unsolvable";
        }
              std::cout << std::endl;
              );
}

int Abstraction::get_peak_memory_estimate() const {
    return peak_memory;
}

void Abstraction::dump_relevant_labels() const {
    std::cout << "relevant labels" << std::endl;
    for (size_t label_no = 0; label_no < relevant_labels.size(); ++label_no) {
        if (label_no) {
            std::cout << label_no << std::endl;
        }
    }
}

void Abstraction::dump() const {
    std::cout << "digraph abstract_transition_graph";
    for (int i: varset)
        std::cout << "_" << i;
    std::cout << " {" << std::endl;
    std::cout << "    node [shape = none] start;" << std::endl;
    for (int i = 0; i < num_states; i++) {
        bool is_init = (i == init_state);
        bool is_goal = goal_states[i];
        std::cout << "    node [shape = " << (is_goal ? "doublecircle" : "circle")
                  << "] node" << i << ";" << std::endl;
        if (is_init)
            std::cout << "    start -> node" << i << ";" << std::endl;
    }

    for (int label_no = 0; label_no < num_labels; label_no++) {
        // reduced labels are automatically skipped because trans is then empty
        const std::vector<AbstractTransition> &trans = transitions_by_label[label_no];
        for (auto t: trans) {
            int src = t.src;
            int target = t.target;
            std::cout << "    node" << src << " -> node" << target << " [label = o_" << label_no << "];"
                      << std::endl;
        }
    }
    std::cout << "}" << std::endl;
}


void Abstraction::dump_names() const {
    std::cout << "digraph abstract_transition_graph";
    for (int i: varset)
        std::cout << "_" << i;
    std::cout << " {" << std::endl;
    std::cout << "    node [shape = none] start;" << std::endl;
    for (int i = 0; i < num_states; i++) {
        bool is_init = (i == init_state);
        bool is_goal = goal_states[i];
        std::cout << "    node [shape = " << (is_goal ? "doublecircle" : "circle")
                  << "] " << description(i) << ";" << std::endl;
        if (is_init)
            std::cout << "    start -> " << description(i) << ";" << std::endl;
    }

    for (int label_no = 0; label_no < num_labels; label_no++) {
        // reduced labels are automatically skipped because trans is then empty
        const std::vector<AbstractTransition> &trans = transitions_by_label[label_no];
        for (auto t: trans) {
            int src = t.src;
            int target = t.target;
            std::cout << "   " << description(src) << " -> " << description(target) << " [label = "
                      << global_simulation_task()->get_operator_name(label_no, false) << "];" << std::endl;
        }
    }
    std::cout << "}" << std::endl;
}

bool Abstraction::is_own_label(int label_no) {
    const std::set<Abstraction *> &relevant_abstractions = labels->get_relevant_for(label_no);
    return relevant_abstractions.size() == 1 && *(relevant_abstractions.begin()) == this;
}

void Abstraction::count_transitions_by_label() {
    assert(is_normalized());
    num_transitions_by_label.resize(num_labels, 0);
    num_goal_transitions_by_label.resize(num_labels, 0);

    for (int i = 0; i < transitions_by_label.size(); ++i) {
        if (transitions_by_label[i].empty())
            continue;
        for (int j = 0; j < transitions_by_label[i].size(); j++) {
            if (transitions_by_label[i][j].target != transitions_by_label[i][j].src) {
                num_transitions_by_label[i]++;
                if (goal_states[transitions_by_label[i][j].target]) {
                    num_goal_transitions_by_label[i]++;
                }
            }
        }
    }
}

void Abstraction::count_transitions
    (const std::vector<Abstraction *> &all_abstractions,
    const std::vector<int> &remaining, bool only_empty,
    bool only_goal, std::vector<int> &result) {
    result.resize(global_simulation_task()->get_num_variables(), 0);
    if (num_transitions_by_label.empty()) {
        count_transitions_by_label();
    }
    for (size_t label_no = 0; label_no < num_labels; ++label_no) {
        int num_tr_label = only_goal ? num_goal_transitions_by_label[label_no] :
            num_transitions_by_label[label_no];
        if (num_tr_label) {
            for (int var: remaining) {
                const Label *l = labels->get_label_by_index(label_no);
                //assert(l->get_relevant_for().size() > 1);
                if ((!only_empty || l->get_relevant_for().size() == 2) &&
                    l->is_relevant_for(all_abstractions[var])) {
                    result[var] += num_tr_label;
                }
            }
        }
    }
}

#ifdef INCLUDE_SYM
void CompositeAbstraction::getAbsStateBDDs(SymVariables *vars,
                                           std::vector<BDD> &abs_bdds) const {
    std::vector<BDD> bdds1, bdds2;
    components[0]->getAbsStateBDDs(vars, bdds1);
    components[1]->getAbsStateBDDs(vars, bdds2);
    for (int i = 0; i < num_states; i++) {
        abs_bdds.push_back(vars->zeroBDD());
    }

    for (int i = 0; i < lookup_table.size(); i++) {
        for (int j = 0; j < lookup_table[i].size(); j++) {
            if (lookup_table[i][j] != -1) {
                abs_bdds[lookup_table[i][j]] += bdds1[i] * bdds2[j];
            }
        }
    }
}

void AtomicAbstraction::getAbsStateBDDs(SymVariables *vars,
                                        std::vector<BDD> &abs_bdds) const {
    for (int i = 0; i < num_states; i++) {
        abs_bdds.push_back(vars->zeroBDD());
    }
    for (int i = 0; i < lookup_table.size(); i++) {
        if (lookup_table[i] != -1) {
            abs_bdds[lookup_table[i]] += vars->preBDD(variable, i);
        }
    }
}

BDD CompositeAbstraction::getIrrelevantStateBDD(SymVariables *vars,
                                                std::vector<BDD> &abs_bdds) const {
    std::vector<BDD> bdds1, bdds2;
    BDD res = components[0]->getIrrelevantStateBDD(vars, bdds1);
    res += components[1]->getIrrelevantStateBDD(vars, bdds2);
    for (int i = 0; i < num_states; i++) {
        abs_bdds.push_back(vars->zeroBDD());
    }

    for (int i = 0; i < lookup_table.size(); i++) {
        for (int j = 0; j < lookup_table[i].size(); j++) {
            int absstate = lookup_table[i][j];
            if (absstate != -1) {
                abs_bdds[absstate] += bdds1[i] * bdds2[j];
            } else {
                res += bdds1[i] * bdds2[j];
            }
        }
    }
    for (int i = 0; i < num_states; i++) {
        if (goal_distances[i] == Abstraction::PRUNED_STATE ||
            init_distances[i] == Abstraction::PRUNED_STATE) {
            res += abs_bdds[i];
        }
    }
    return res;
}

BDD AtomicAbstraction::getIrrelevantStateBDD(SymVariables *vars,
                                             std::vector<BDD> &abs_bdds) const {
    for (int i = 0; i < num_states; i++) {
        abs_bdds.push_back(vars->zeroBDD());
    }
    BDD res = vars->zeroBDD();
    for (int i = 0; i < lookup_table.size(); i++) {
        if (lookup_table[i] != -1) {
            abs_bdds[lookup_table[i]] += vars->preBDD(variable, i);
        } else {
            res += vars->preBDD(variable, i);
        }
    }
    for (int i = 0; i < num_states; i++) {
        if (goal_distances[i] == Abstraction::PRUNED_STATE ||
            init_distances[i] == Abstraction::PRUNED_STATE) {
            res += abs_bdds[i];
        }
    }

    return res;
}
#endif


// Methods to prune irrelevant transitions Prune all the
// transitions of a given label (it is completely dominated by
// some other label or by noop)
unsigned int Abstraction::prune_transitions_dominated_label_all(int label_no /*, const LabelMap & label_map*/) {
    // PIET-edit: num should only be the number of pruned transitions, i.e., those with the correct label, right?
    //int num = transitions_by_label.size();
    unsigned int num = transitions_by_label[label_no].size();
    if (num > 0) {
        std::vector<AbstractTransition>().swap(transitions_by_label[label_no]);
        if (store_original_operators)
            std::vector<boost::dynamic_bitset<>>().swap(transitions_by_label_based_on_operators[label_no]);
        clear_distances();
        //->kill_label(label_map.get_id(label_no));
    }

    return num;
}

// Prune all the transitions of label_no such that exist a better
// transition for label_no_by
int Abstraction::prune_transitions_dominated_label(int lts_id,
                                                   const std::vector<LabelledTransitionSystem *> &ltss,
                                                   const DominanceRelation &domrel,
                                                   const LabelMap &labelMap,
                                                   int label_no, int label_no_by) {
    int count = 0, count_good = 0;
    int label_id = labelMap.get_id(label_no);
    const SimulationRelation &rel = domrel[lts_id];
    unsigned int num = transitions_by_label[label_no].size();
    transitions_by_label[label_no].erase(
        std::remove_if(std::begin(transitions_by_label[label_no]),
                       std::end(transitions_by_label[label_no]),
                       [&](AbstractTransition &t) {
                           bool res = std::find_if(
                               std::begin(transitions_by_label[label_no_by]),
                               std::end(transitions_by_label[label_no_by]),
                               [&](AbstractTransition &t2) {
                                   return t2.src == t.src &&
                                   rel.simulates(t2.target, t.target);
                               }) != std::end(transitions_by_label[label_no_by]) &&
                               domrel.propagate_transition_pruning(lts_id, ltss, t.src, label_id,
                                                                   t.target);

                           if (store_original_operators) {
                               if (!res) {
                                   if (count != count_good)
                                       transitions_by_label_based_on_operators[label_no][count_good] =
                                           transitions_by_label_based_on_operators[label_no][count];
                                   count_good++;
                               }
                               count++;
                           }

                           return res;
                       }),
        transitions_by_label[label_no].end());

    if (store_original_operators && count_good < count) {
        transitions_by_label_based_on_operators[label_no].erase(
            std::begin(transitions_by_label_based_on_operators[label_no]) + count_good,
            std::end(transitions_by_label_based_on_operators[label_no]));
    }

    if (transitions_by_label[label_no].size() != num)
        clear_distances();
    return num - transitions_by_label[label_no].size();
}

// Prune all the transitions of label_no such that exist a better
// transition for label_no_by
int Abstraction::prune_transitions_dominated_label_equiv(int lts_id,
                                                         const std::vector<LabelledTransitionSystem *> &ltss,
                                                         const DominanceRelation &domrel,
                                                         const LabelMap &labelMap,
                                                         int label_no, int label_no2) {
    int label_id = labelMap.get_id(label_no);
    int label_id2 = labelMap.get_id(label_no2);
    const SimulationRelation &rel = domrel[lts_id];
    int num = transitions_by_label[label_no].size() + transitions_by_label[label_no2].size();
    int count = 0, count_good = 0;

    if (label_no == label_no2) {
        //Added special case when the labels are the same. This one
        //uses remove_copy_if instead of remove_if because its
        //safer. remove_if still works but may depend on the compiler
        //or the class definition of the transitions.

        transitions_by_label[label_no].erase(
            std::remove_copy_if(std::begin(transitions_by_label[label_no]),
                                std::end(transitions_by_label[label_no]),
                                std::begin(transitions_by_label[label_no]),
                                [&](AbstractTransition &t) {
                                    bool res = std::find_if(
                                        std::begin(transitions_by_label[label_no2]),
                                        std::end(transitions_by_label[label_no2]),
                                        [&](AbstractTransition &t2) {
                                            /* PIET-edit: Make sure that not both, the target states and the labels, are the same */
                                            return t2.src == t.src &&
                                            rel.simulates(t2.target, t.target) &&
                                            (!rel.simulates(t.target, t2.target) ||
                                             t.target > t2.target);
                                        }) != std::end(transitions_by_label[label_no2])
                                        && domrel.propagate_transition_pruning(
                                        lts_id, ltss, t.src, label_id, t.target);

                                    if (store_original_operators) {
                                        if (!res) {
                                            if (count != count_good)
                                                transitions_by_label_based_on_operators[label_no][count_good] =
                                                    transitions_by_label_based_on_operators[label_no][count];
                                            count_good++;
                                        }
                                        count++;
                                    }

                                    return res;
                                }),
            transitions_by_label[label_no].end());

        if (store_original_operators && count_good < count) {
            transitions_by_label_based_on_operators[label_no].erase(
                std::begin(transitions_by_label_based_on_operators[label_no]) + count_good,
                std::end(transitions_by_label_based_on_operators[label_no]));
        }
    } else {
        transitions_by_label[label_no].erase(
            std::remove_if(std::begin(transitions_by_label[label_no]),
                           std::end(transitions_by_label[label_no]),
                           [&](AbstractTransition &t) {
                               bool res = std::find_if(std::begin(transitions_by_label[label_no2]),
                                                       std::end(transitions_by_label[label_no2]),
                                                       [&](AbstractTransition &t2) {
                                                           /* PIET-edit: Make sure that not both, the target states and the labels, are the same */
                                                           return t2.src == t.src &&
                                                           rel.simulates(t2.target, t.target) &&
                                                           (!rel.simulates(t.target, t2.target) ||
                                                            label_no >
                                                            label_no2 /* || (label_no==label_no2 && t.target > t2.target)*/);
                                                       }) != std::end(transitions_by_label[label_no2]) &&
                                   domrel.propagate_transition_pruning(lts_id, ltss, t.src, label_id,
                                                                       t.target);

                               if (store_original_operators) {
                                   if (!res) {
                                       if (count != count_good)
                                           transitions_by_label_based_on_operators[label_no][count_good] =
                                               transitions_by_label_based_on_operators[label_no][count];
                                       count_good++;
                                   }
                                   count++;
                               }

                               return res;
                           }),
            transitions_by_label[label_no].end());

        if (store_original_operators && count_good < count) {
            transitions_by_label_based_on_operators[label_no].erase(
                std::begin(transitions_by_label_based_on_operators[label_no]) + count_good,
                std::end(transitions_by_label_based_on_operators[label_no]));
        }

        count_good = 0;
        count = 0;

        transitions_by_label[label_no2].erase(
            std::remove_if(std::begin(transitions_by_label[label_no2]),
                           std::end(transitions_by_label[label_no2]),
                           [&](AbstractTransition &t) {
                               bool res = std::find_if(std::begin(transitions_by_label[label_no]),
                                                       std::end(transitions_by_label[label_no]),
                                                       [&](AbstractTransition &t2) {
                                                           /* PIET-edit: Make sure that not both, the target states and the labels, are the same */
                                                           return t2.src == t.src &&
                                                           rel.simulates(t2.target, t.target) &&
                                                           (!rel.simulates(t.target, t2.target) ||
                                                            label_no2 > label_no);
                                                       }) != std::end(transitions_by_label[label_no]) &&
                                   domrel.propagate_transition_pruning(lts_id, ltss, t.src, label_id2,
                                                                       t.target);


                               if (store_original_operators) {
                                   if (!res) {
                                       if (count != count_good)
                                           transitions_by_label_based_on_operators[label_no2][count_good] =
                                               transitions_by_label_based_on_operators[label_no2][count];
                                       count_good++;
                                   }
                                   count++;
                               }
                               return res;
                           }),
            transitions_by_label[label_no2].end());


        if (store_original_operators && count_good < count) {
            transitions_by_label_based_on_operators[label_no2].erase(
                std::begin(transitions_by_label_based_on_operators[label_no2]) + count_good,
                std::end(transitions_by_label_based_on_operators[label_no2]));
        }
    }


    if (transitions_by_label[label_no].size() + transitions_by_label[label_no2].size() != num)
        clear_distances();
    return num - transitions_by_label[label_no].size() - transitions_by_label[label_no2].size();
}

//Prune all the transitions dominated by noop
int Abstraction::prune_transitions_dominated_label_noop(int lts_id,
                                                        const std::vector<LabelledTransitionSystem *> &ltss,
                                                        const DominanceRelation &domrel,
                                                        const LabelMap &labelMap,
                                                        int label_no) {
    //Timer t;
    int label_id = labelMap.get_id(label_no);
    const SimulationRelation &rel = domrel[lts_id];
    int num = transitions_by_label[label_no].size();

    int count = 0, count_good = 0;
    transitions_by_label[label_no].erase(
        std::remove_if(std::begin(transitions_by_label[label_no]),
                       std::end(transitions_by_label[label_no]),
                       [&](AbstractTransition &t) {
                           bool res = rel.simulates(t.src, t.target) &&
                               domrel.propagate_transition_pruning(lts_id, ltss, t.src, label_id,
                                                                   t.target);
                           if (store_original_operators) {
                               if (!res) {
                                   if (count != count_good)
                                       transitions_by_label_based_on_operators[label_no][count_good] =
                                           transitions_by_label_based_on_operators[label_no][count];
                                   count_good++;
                               }
                               count++;
                           }
                           return res;
                       }),
        transitions_by_label[label_no].end());
    if (store_original_operators && count_good < count) {
        transitions_by_label_based_on_operators[label_no].erase(
            std::begin(transitions_by_label_based_on_operators[label_no]) + count_good,
            std::end(transitions_by_label_based_on_operators[label_no]));
    }
    if (transitions_by_label[label_no].size() != num)
        clear_distances();
    //cout << "Calling prune transitions dominated by noop with " << label_no  << " in LTS " << lts_id << " has " << num << " transitions" << " and took " << t() << std::endl;
    return num - transitions_by_label[label_no].size();
}


void PDBAbstraction::apply_abstraction_to_lookup_table(
    const std::vector<AbstractStateRef> &abstraction_mapping) {
    DEBUG_MAS(std::cout << tag() << "applying abstraction to lookup table" << std::endl;
              );
    for (int &table_entry: lookup_table) {
        AbstractStateRef old_state = table_entry;
        if (old_state != PRUNED_STATE)
            table_entry = abstraction_mapping[old_state];
    }
}

PDBAbstraction::PDBAbstraction(Labels *labels, std::vector<int> pattern_)
    : Abstraction(labels), pattern(std::move(pattern_)) {
    varset.insert(varset.end(), pattern.begin(), pattern.end());

    goal_relevant_vars = 0;
    for (int goal_no = 0; goal_no < global_simulation_task()->get_num_goals(); goal_no++) {
        int goal_v = global_simulation_task()->get_goal_fact(goal_no).var;
        if (std::find(std::begin(pattern), std::end(pattern), goal_v) != std::end(pattern)) {
            goal_relevant_vars++;
        }
    }
    all_goals_relevant = goal_relevant_vars == global_simulation_task()->get_num_goals();

    num_states = 1;
    for (int v: pattern)
        num_states *= global_simulation_task()->get_variable_domain_size(v);

    lookup_table.reserve(num_states);
    for (int i = 0; i < num_states; i++) {
        lookup_table.push_back(i);
    }
    init_state = rank(global_simulation_task()->get_initial_state_values());
    goal_states.resize(num_states, false);
    // Set goals
    std::vector<int> goal_vals(global_simulation_task()->get_num_variables(), -1);
    for (int goal_no = 0; goal_no < global_simulation_task()->get_num_goals(); ++goal_no) {
        const auto &goal = global_simulation_task()->get_goal_fact(goal_no);
        goal_vals[goal.var] = goal.value;
    }
    insert_goals(goal_vals, 0);

    // Set transitions
    for (int label_no = 0; label_no < labels->get_size(); label_no++) {
        const Label *label = labels->get_label_by_index(label_no);
        std::vector<int> pre_vals(global_simulation_task()->get_num_variables(), -1);
        std::vector<int> eff_vals(global_simulation_task()->get_num_variables(), -1);
        const std::vector<Prevail> &prevs = label->get_prevail();
        for (auto prev: prevs) {
            pre_vals[prev.var] = prev.prev;
            eff_vals[prev.var] = prev.prev;
        }
        const std::vector<PrePost> &pre_posts = label->get_pre_post();
        for (const auto &pre_post: pre_posts) {
            pre_vals[pre_post.var] = pre_post.pre;
            eff_vals[pre_post.var] = pre_post.post;
        }
        //Check whether is relevant for patter
        bool is_relevant = false;
        for (int v: pattern) {
            if (pre_vals[v] != -1 || eff_vals[v] != -1) {
                is_relevant = true;
            }
        }
        if (!is_relevant)
            continue;

        relevant_labels[label_no] = true;
        labels->set_relevant_for(label_no, this);

        insert_transitions(pre_vals, eff_vals, label_no, 0);

        if (!are_transitions_sorted_unique())
            normalize();
    }
}


void PDBAbstraction::insert_transitions(std::vector<int> &pre_vals, std::vector<int> &eff_vals,
                                        int label_no, int pos) {
    if (pos == pattern.size()) {
        AbstractTransition trans(rank(pre_vals), rank(eff_vals));
        transitions_by_label[label_no].push_back(trans);
        return;
    }
    int v = pattern[pos];
    if (pre_vals[v] == -1) {
        bool change_eff = (eff_vals[v] == -1);
        for (int val = 0; val < global_simulation_task()->get_variable_domain_size(v); val++) {
            pre_vals[v] = val;
            if (change_eff)
                eff_vals[v] = val;
            insert_transitions(pre_vals, eff_vals, label_no, pos + 1);
        }
        pre_vals[v] = -1;
        if (change_eff)
            eff_vals[v] = -1;
    } else {
        insert_transitions(pre_vals, eff_vals, label_no, pos + 1);
    }
}

void PDBAbstraction::insert_goals(std::vector<int> &goal_vals, int pos) {
    if (pos == pattern.size()) {
        goal_states[rank(goal_vals)] = true;
        return;
    }
    int v = pattern[pos];
    if (goal_vals[v] == -1) {
        for (int val = 0; val < global_simulation_task()->get_variable_domain_size(v); val++) {
            goal_vals[v] = val;
            insert_goals(goal_vals, pos + 1);
        }
        goal_vals[v] = -1;
    } else {
        insert_goals(goal_vals, pos + 1);
    }
}

std::string PDBAbstraction::description() const {
    std::ostringstream s;
    s << "PDB abstraction (" << pattern.size() << ")";
    return s.str();
}

std::string PDBAbstraction::description(int s) const {
    std::stringstream ss;

    for (int i = pattern.size() - 1; i >= 0; --i) {
        int v = pattern[i];
        int val = s % global_simulation_task()->get_variable_domain_size(v);
        s = s / global_simulation_task()->get_variable_domain_size(v);
        ss << global_simulation_task()->get_fact_name(FactPair(v, val)) << " ";
    }
    return ss.str();
}

unsigned int PDBAbstraction::memory_estimate() const {
    unsigned int result = Abstraction::memory_estimate();
    result += sizeof(PDBAbstraction) - sizeof(Abstraction);
    result += sizeof(AbstractStateRef) * lookup_table.capacity();
    return result;
}

AbstractStateRef PDBAbstraction::get_abstract_state(const State &state) const {
    return lookup_table[rank(state)];
}

AbstractStateRef PDBAbstraction::get_abstract_state(const std::vector<int> &state) const {
    return lookup_table[rank(state)];
}

#ifdef INCLUDE_SYM
void PDBAbstraction::getAbsStateBDDs(SymVariables *vars,
                                     std::vector<BDD> &abs_bdds) const {
    for (int i = 0; i < num_states; i++) {
        abs_bdds.push_back(vars->zeroBDD());
    }
    for (int i = 0; i < num_states; i++) {
        if (lookup_table[i] != -1) {
            abs_bdds[lookup_table[i]] += unrankBDD(vars, i);
        }
    }
}


BDD PDBAbstraction::getIrrelevantStateBDD(SymVariables *vars,
                                          std::vector<BDD> &abs_bdds) const {
    for (int i = 0; i < num_states; i++) {
        abs_bdds.push_back(vars->zeroBDD());
    }
    BDD res = vars->zeroBDD();
    for (int i = 0; i < num_states; i++) {
        if (lookup_table[i] != -1) {
            abs_bdds[lookup_table[i]] += unrankBDD(vars, i);
        } else {
            res += unrankBDD(vars, i);
        }
    }
    for (int i = 0; i < num_states; i++) {
        if (goal_distances[i] == Abstraction::PRUNED_STATE ||
            init_distances[i] == Abstraction::PRUNED_STATE) {
            res += abs_bdds[i];
        }
    }

    return res;
}

BDD PDBAbstraction::unrankBDD(SymVariables *vars, int id) const {
    BDD res = vars->oneBDD();
    for (int i = pattern.size() - 1; i >= 0; --i) {
        int v = pattern[i];
        int val = id % global_simulation_task()->get_variable_domain_size(v);
        id = id / global_simulation_task()->get_variable_domain_size(v);
        res *= vars->preBDD(v, val);
    }
    return res;
}
#endif

LabelledTransitionSystem *Abstraction::get_lts(const LabelMap &labelMap) {
    if (!lts) {
        lts = std::make_unique<LabelledTransitionSystem>(this, labelMap);
    }
    return lts.get();
}


int Abstraction::estimate_transitions(const Abstraction *other) const {
    int num_total = 0;
    for (size_t label_no = 0; label_no < transitions_by_label.size(); ++label_no) {
        if (relevant_labels[label_no] || other->relevant_labels[label_no]) {
            int num_mine = (relevant_labels[label_no] ? transitions_by_label[label_no].size() : num_states);
            int num_other = (other->relevant_labels[label_no] ?
                             other->transitions_by_label[label_no].size() :
                             other->num_states);
            num_total += num_mine * num_other;
        }
    }
    return num_total;
}


void Abstraction::get_dead_labels(std::vector<bool> &dead_labels,
                                  std::vector<int> &new_dead_labels) {
    for (int i = 0; i < labels->get_size(); i++) {
        if (dead_labels[i])
            continue;     // corresponding operators must already have been extracted somewhere else
        if (labels->is_label_reduced(i))
            continue;
        if (relevant_labels[i] && transitions_by_label[i].empty()) {
            dead_labels[i] = true;
            new_dead_labels.push_back(i);
        }
    }
}


bool Abstraction::check_dead_operators(std::vector<bool> &dead_labels, std::vector<bool> &dead_operators) const {
    bool ret = false;
    //cout << dead_labels.size() << " / " << labels->get_size() << std::endl;
    for (int i = 0; i < labels->get_size(); i++) {
        if (dead_labels[i])
            continue;     // corresponding operators must already have been extracted somewhere else
        if (labels->is_label_reduced(i))
            continue;
        if (relevant_labels[i] && transitions_by_label[i].empty()) {
            dead_labels[i] = true;
            const Label *label = labels->get_label_by_index(i);
            std::set<int> op_ids;
            label->get_operators(op_ids);
            for (auto id: op_ids) {
                if (!dead_operators[id])
                    ret = true;
                dead_operators[id] = true;
            }
        }
    }
    return ret;
}


void Abstraction::reset_lts() {
    lts.reset();
}


Abstraction::Abstraction(const Abstraction &o) :
    labels(o.labels), num_labels(o.num_labels),
    transitions_by_label(o.transitions_by_label),
    transitions_by_label_based_on_operators(o.transitions_by_label_based_on_operators),
    relevant_labels(o.relevant_labels),
    num_transitions_by_label(o.num_transitions_by_label),
    num_goal_transitions_by_label(o.num_goal_transitions_by_label),
    num_states(o.num_states),
    init_distances(o.init_distances),
    goal_distances(o.goal_distances),
    goal_states(o.goal_states),
    init_state(o.init_state),
    max_f(o.max_f),
    max_g(o.max_g),
    max_h(o.max_h),
    transitions_sorted_unique(o.transitions_sorted_unique),
    goal_relevant_vars(o.goal_relevant_vars),
    all_goals_relevant(o.all_goals_relevant),
    simulation_relation(nullptr), varset(o.varset) {
}


Abstraction *AtomicAbstraction::clone() const {
    return new AtomicAbstraction(*this);
}

Abstraction *CompositeAbstraction::clone() const {
    return new CompositeAbstraction(*this);
}

Abstraction *PDBAbstraction::clone() const {
    return new PDBAbstraction(*this);
}


CompositeAbstraction::CompositeAbstraction(const CompositeAbstraction &o) :
    Abstraction(o), lookup_table(o.lookup_table) {
    components[0] = o.components[0];
    components[1] = o.components[1];
}
}
