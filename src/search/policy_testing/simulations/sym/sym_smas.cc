#include "sym_smas.h"

#include "../merge_and_shrink/label_reducer.h"
#include "smas_shrink_fh.h"
#include "../utils/priority_queue.h"
#include "sym_transition.h"
#include "sym_util.h"
#include "smas_abs_state.h"
#include "smas_shrink_state.h"
#include "../utils/debug.h"

#include <sstream>

namespace simulations {
const int SymSMAS::PRUNED_STATE = -1;
const int SymSMAS::DISTANCE_UNKNOWN = -2;

// inline int get_op_index(const Operator *op) {
/* TODO: The op_index computation is duplicated from
       LabelReducer::get_op_index() and actually belongs neither
       here nor there. There should be some canonical way of getting
       from an Operator pointer to an index, but it's not clear how to
       do this in a way that best fits the overall planner
       architecture (taking into account axioms etc.) */
//    int op_index = op - &*g_operators.begin();
//    assert(op_index >= 0 && op_index < global_simulation_task()->get_num_operators());
//    return op_index;
//}


SymSMAS::SymSMAS(SymVariables *bdd_vars, bool is_unit_cost_,
                 OperatorCost cost_type_) :
    SymAbstraction(bdd_vars, AbsTRsStrategy::SHRINK_AFTER_IMG), is_unit_cost(is_unit_cost_),
    cost_type(cost_type_),
    absVarsCube(bdd_vars->oneBDD()),        // biimpAbsStateVars(bdd_vars->oneBDD()),
    absVarsCubep(bdd_vars->oneBDD()),
    are_labels_reduced(false), f_preserved(true), peak_memory(0) {
    //Every variable is non abstracted
    for (int i = 0; i < global_simulation_task()->get_num_variables(); i++) {
        fullVars.insert(i);
    }

    clear_distances();
    transitions_by_op.resize(global_simulation_task()->get_num_operators());
    for (int i = 0; i < global_simulation_task()->get_num_operators(); ++i) {
        irrelevant_operators.emplace_back(i);
    }
    num_states = 1;
    goal_states.resize(num_states, true);

    auto ss = std::make_shared<SMASShrinkState>(vars);
    shrinkStates.push_back(ss);
    absStates.reserve(num_states);
    absStates.push_back(std::make_shared<SMASAbsState>(ss, vars));
}


SymSMAS::SymSMAS(SymVariables *bdd_vars, bool is_unit_cost_, OperatorCost cost_type_, int variable) :
    SymAbstraction(bdd_vars, AbsTRsStrategy::SHRINK_AFTER_IMG),
    is_unit_cost(is_unit_cost_), cost_type(cost_type_),
    absVarsCube(bdd_vars->oneBDD()),
    are_labels_reduced(false), f_preserved(true), peak_memory(0) {
    clear_distances();
    transitions_by_op.resize(global_simulation_task()->get_num_operators());

    absVars.insert(variable);
    for (int i = 0; i < global_simulation_task()->get_num_variables(); i++) {
        if (i != variable) {
            fullVars.insert(i);
        }
    }

    /*
      This generates the states of the atomic abstraction, but not the
      arcs: It is more efficient to generate all arcs of all atomic
      abstractions simultaneously.
    */
    int range = global_simulation_task()->get_variable_domain_size(variable);

    int init_value = (global_simulation_task()->get_initial_state_values())[variable];
    int goal_value = -1;
    for (int goal_no = 0; goal_no < global_simulation_task()->get_num_goals(); goal_no++) {
        auto goal = global_simulation_task()->get_goal_fact(goal_no);
        if (goal.var == variable) {
            assert(goal_value == -1);
            goal_value = goal.value;
        }
    }

    auto ss = std::make_shared<SMASShrinkState>(vars);
    shrinkStates.push_back(ss);
    //std::cout << "CREATED EMPTY: " << ss << std::endl;
    num_states = range;
    goal_states.resize(num_states, false);
    absStates.reserve(num_states);
    for (int value = 0; value < range; value++) {
        if (value == goal_value || goal_value == -1) {
            goal_states[value] = true;
        }
        if (value == init_value)
            init_state = value;
        absStates.push_back(std::make_shared<SMASAbsState>(ss, vars, variable, value));
    }
}

SymSMAS::SymSMAS(SymSMAS *abs1, SymSMAS *abs2, AbsTRsStrategy absTRsStrategy,
                 const std::vector<BDD> &notMutexBDDs) :
    SymAbstraction(abs1->vars, absTRsStrategy), is_unit_cost(abs1->is_unit_cost),
    cost_type(abs1->cost_type),
    are_labels_reduced(false), f_preserved(true),
    peak_memory(0) {
    clear_distances();
    transitions_by_op.resize(global_simulation_task()->get_num_operators());

    DEBUG_MSG(std::cout << "Merging " << abs1->tag() << " and " << abs2->tag() << std::endl;
              );

    assert(abs1->is_solvable() && abs2->is_solvable());

    std::set_union(abs1->absVars.begin(), abs1->absVars.end(), abs2->absVars.begin(),
                   abs2->absVars.end(), inserter(absVars, absVars.end()));

    for (int i = 0; i < global_simulation_task()->get_num_variables(); i++) {
        if (!absVars.count(i)) {
            fullVars.insert(i);
        }
    }

    absVarsCube = abs1->absVarsCube * abs2->absVarsCube;
    absVarsCubep = abs1->absVarsCubep * abs2->absVarsCubep;
    num_states = abs1->size() * abs2->size();
    goal_states.resize(num_states, false);
    absStates.resize(num_states);
    shrinkStates.reserve(num_states);
    std::map<std::pair<SMASShrinkState *, SMASShrinkState *>,
             std::shared_ptr<SMASShrinkState>> table;
    std::set<std::shared_ptr<SMASShrinkState>> table2;
    for (int s1 = 0; s1 < abs1->size(); s1++) {
        for (int s2 = 0; s2 < abs2->size(); s2++) {
            int state = s1 * abs2->size() + s2;
            auto ss1 = abs1->absStates[s1]->shrinkState;
            auto ss2 = abs2->absStates[s2]->shrinkState;
            std::shared_ptr<SMASShrinkState> shrinkState;
            if (ss1->cube.IsOne()) {
                shrinkState = ss2;
                if (!table2.count(shrinkState)) {
                    shrinkStates.push_back(shrinkState);
                    table2.insert(shrinkState);
                }
            } else if (ss2->cube.IsOne()) {
                shrinkState = ss1;
                if (!table2.count(shrinkState)) {
                    shrinkStates.push_back(shrinkState);
                    table2.insert(shrinkState);
                }
            } else {
                auto pairSS = std::make_pair(ss1.get(), ss2.get());
                if (!table.count(pairSS)) {
                    auto ssptr = std::make_shared<SMASShrinkState>(ss1, ss2);
                    shrinkStates.push_back(ssptr);
                    table[pairSS] = ssptr;
                    shrinkState = ssptr;
                } else {
                    shrinkState = table[pairSS];
                }
            }
            absStates[state] =
                std::make_shared<SMASAbsState>(shrinkState,
                                               vars, abs1->absStates[s1],
                                               abs2->absStates[s2], notMutexBDDs);

            if (abs1->goal_states[s1] && abs2->goal_states[s2])
                goal_states[state] = true;
            if (s1 == abs1->init_state && s2 == abs2->init_state)
                init_state = state;
        }
    }
    DEBUG_MSG(std::cout << "Finished creation of abstract states" << std::endl;
              );
    DEBUG_MSG(std::cout << count_spurious_states() << " of " << absStates.size() << " states detected spurious"
                        << std::endl;
              );


    for (auto &relevant_operator : abs1->relevant_operators)
        set_marker_1(relevant_operator.get_index(), true);
    for (auto &relevant_operator : abs2->relevant_operators)
        set_marker_2(relevant_operator.get_index(), true);

    int multiplier = abs2->size();
    for (int op_no = 0; op_no < global_simulation_task()->get_num_operators(); op_no++) {
        // There are no more spurious operators anymore (removed in preprocess)
        // if(op->spurious) continue;
        bool relevant1 = get_marker_1(op_no);
        bool relevant2 = get_marker_2(op_no);
        if (relevant1 || relevant2) {
            std::vector<AbstractTransition> &transitions = transitions_by_op[op_no];
            relevant_operators.emplace_back(op_no);
            const std::vector<AbstractTransition> &bucket1 =
                abs1->transitions_by_op[op_no];
            const std::vector<AbstractTransition> &bucket2 =
                abs2->transitions_by_op[op_no];
            if (relevant1 && relevant2) {
                transitions.reserve(bucket1.size() * bucket2.size());
                for (const auto &i: bucket1) {
                    int src1 = i.src;
                    int target1 = i.target;
                    for (const auto &j: bucket2) {
                        int src2 = j.src;
                        int target2 = j.target;
                        int src = src1 * multiplier + src2;
                        int target = target1 * multiplier + target2;
                        if (!(absStates[src]->is_spurious()) &&
                            !(absStates[target]->is_spurious())) {
                            transitions.emplace_back(src, target);
                        }
                    }
                }
            } else if (relevant1) {
                assert(!relevant2);
                transitions.reserve(bucket1.size() * abs2->size());
                for (const auto &i: bucket1) {
                    int src1 = i.src;
                    int target1 = i.target;
                    for (int s2 = 0; s2 < abs2->size(); s2++) {
                        int src = src1 * multiplier + s2;
                        int target = target1 * multiplier + s2;
                        if (!(absStates[src]->is_spurious()) &&
                            !(absStates[target]->is_spurious())) {
                            transitions.emplace_back(src, target);
                        }
                    }
                }
            } else if (relevant2) {
                assert(!relevant1);
                transitions.reserve(bucket2.size() * abs1->size());
                for (const auto &i: bucket2) {
                    int src2 = i.src;
                    int target2 = i.target;
                    for (int s1 = 0; s1 < abs1->size(); s1++) {
                        int src = s1 * multiplier + src2;
                        int target = s1 * multiplier + target2;
                        if (!(absStates[src]->is_spurious()) &&
                            !(absStates[target]->is_spurious())) {
                            transitions.emplace_back(src, target);
                        }
                    }
                }
            }
        } else {
            //Not relevant
            irrelevant_operators.emplace_back(op_no);
            /*	  std::cout << "Abstraction " << tag() << " has irrelevant op: ";
                op->dump();*/
        }
    }
    DEBUG_MSG(std::cout << "Finished creation of transitions" << std::endl;
              );

    for (auto &relevant_operator : abs1->relevant_operators)
        set_marker_1(relevant_operator.get_index(), false);
    for (auto &relevant_operator : abs2->relevant_operators)
        set_marker_2(relevant_operator.get_index(), false);

    //setAbsBDDsp();
}

std::string SymSMAS::tag() const {
    std::stringstream result;
    result << "SMAS [" << fullVars.size() << ", " << absVars.size() << "]";
    /*std::copy(absVars.begin(), absVars.end()-1,
      std::ostream_iterator<int>(result, " "));*/
    //result << *absVars.begin() << "-" << absVars.back() << "(" << absVars.size() << ")] ";
    return result.str();
}


void SymSMAS::clear_distances() {
    max_f = DISTANCE_UNKNOWN;
    max_g = DISTANCE_UNKNOWN;
    max_h = DISTANCE_UNKNOWN;
    init_distances.clear();
    goal_distances.clear();
}

int SymSMAS::size() const {
    return num_states;
}

int SymSMAS::get_max_f() const {
    return max_f;
}

int SymSMAS::get_max_g() const {
    return max_g;
}

int SymSMAS::get_max_h() const {
    return max_h;
}

void SymSMAS::compute_distances() {
    std::cout << tag() << std::flush;
    if (max_h != DISTANCE_UNKNOWN) {
        std::cout << "distances already known, max_h=" << max_h << std::endl;
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
    if (is_unit_cost) {
        std::cout << "computing distances using unit-cost algorithm" << std::endl;
        compute_init_distances_unit_cost();
        compute_goal_distances_unit_cost();
    } else {
        std::cout << "computing distances using general-cost algorithm" << std::endl;
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
        SMASShrinkStrategy *shrink_temp = SMASShrinkFH::create_default(num_states);
        shrink_temp->shrink(*this, num_states, true);
        delete shrink_temp;
    }
}

int SymSMAS::get_cost_for_op(int op_no) const {
    return get_adjusted_action_cost(get_op_proxy(op_no), cost_type, has_unit_cost());
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

void SymSMAS::compute_init_distances_unit_cost() {
    std::vector<std::vector<AbstractStateRef>> forward_graph(num_states);
    for (auto &transitions: transitions_by_op) {
        for (auto trans: transitions) {
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

void SymSMAS::compute_goal_distances_unit_cost() {
    std::vector<std::vector<AbstractStateRef>> backward_graph(num_states);
    for (auto &transitions: transitions_by_op) {
        for (auto trans: transitions) {
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

static void dijkstra_search(const std::vector<std::vector<std::pair<int, int>>> &graph,
                            AdaptiveQueue<int, AbstractStateRef> &queue, std::vector<int> &distances) {
    while (!queue.empty()) {
        std::pair<int, int> top_pair = queue.pop();
        int distance = top_pair.first;
        int state = top_pair.second;
        int state_distance = distances[state];
        assert(state_distance <= distance);
        if (state_distance < distance)
            continue;
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

void SymSMAS::compute_init_distances_general_cost() {
    std::vector<std::vector<std::pair<int, int>>> forward_graph(num_states);
    for (int i = 0; i < transitions_by_op.size(); i++) {
        int op_cost = get_cost_for_op(i);
        const std::vector<AbstractTransition> &transitions = transitions_by_op[i];
        for (const auto &trans: transitions) {
            forward_graph[trans.src].push_back(std::make_pair(trans.target, op_cost));
        }
    }

    // TODO: Reuse the same queue for multiple computations to save speed?
    //       Also see compute_goal_distances_general_cost.
    AdaptiveQueue<int, AbstractStateRef> queue;
    for (AbstractStateRef state = 0; state < num_states; state++) {
        if (state == init_state) {
            init_distances[state] = 0;
            queue.push(0, state);
        }
    }
    dijkstra_search(forward_graph, queue, init_distances);
}

void SymSMAS::compute_goal_distances_general_cost() {
    std::vector<std::vector<std::pair<int, int>>> backward_graph(num_states);
    for (int i = 0; i < transitions_by_op.size(); i++) {
        int op_cost = get_cost_for_op(i);
        const std::vector<AbstractTransition> &transitions = transitions_by_op[i];
        for (const auto &trans: transitions) {
            backward_graph[trans.target].push_back(std::make_pair(trans.src, op_cost));
        }
    }

    // TODO: Reuse the same queue for multiple computations to save speed?
    //       Also see compute_init_distances_general_cost.
    AdaptiveQueue<int, AbstractStateRef> queue;
    for (AbstractStateRef state = 0; state < num_states; state++) {
        if (goal_states[state]) {
            goal_distances[state] = 0;
            queue.push(0, state);
        }
    }
    dijkstra_search(backward_graph, queue, goal_distances);
}

/*
    void SymSMAS::normalize(bool reduce_labels) {
        // Apply label reduction and remove duplicate transitions.

        // dump();

        std::cout << tag() << "normalizing ";

        LabelReducer *reducer = nullptr;
        if (reduce_labels) {
            if (are_labels_reduced) {
                std::cout << "without label reduction (already reduced)" << std::endl;
            } else {
                std::cout << "with label reduction" << std::endl;
                reducer = new LabelReducer(relevant_operators, absVars, cost_type);
                reducer->statistics();
                are_labels_reduced = true;
            }
        } else {
            std::cout << "without label reduction" << std::endl;
        }

        typedef std::vector<std::pair<AbstractStateRef, int>> StateBucket;

        // First, partition by target state. Also replace operators by
        // their canonical representatives via label reduction and clear
        // away the transitions that have been processed.
        std::vector<StateBucket> target_buckets(num_states);
        for (int op_no = 0; op_no < transitions_by_op.size(); op_no++) {
            std::vector<AbstractTransition> &transitions = transitions_by_op[op_no];
            if (!transitions.empty()) {
                int reduced_op_no;
                if (reducer) {
                    const Operator *op = &g_operators[op_no];
                    const Operator *reduced_op = reducer->get_reduced_label(op);
                    reduced_op_no = get_op_index(reduced_op);
                } else {
                    reduced_op_no = op_no;
                }
                for (auto &t: transitions) {
                    target_buckets[t.target].push_back(std::make_pair(t.src, reduced_op_no));
                }
                std::vector<AbstractTransition>().swap(transitions);
            }
        }

        // Second, partition by src state.
        std::vector<StateBucket> src_buckets(num_states);

        for (AbstractStateRef target = 0; target < num_states; target++) {
            StateBucket &bucket = target_buckets[target];
            for (auto &item: bucket) {
                AbstractStateRef src = item.first;
                int op_no = item.second;
                src_buckets[src].push_back(std::make_pair(target, op_no));
            }
        }
        std::vector<StateBucket>().swap(target_buckets);

        // Finally, partition by operator and drop duplicates.
        for (AbstractStateRef src = 0; src < num_states; src++) {
            StateBucket &bucket = src_buckets[src];
            for (auto &item: bucket) {
                int target = item.first;
                int op_no = item.second;

                std::vector<AbstractTransition> &op_bucket = transitions_by_op[op_no];
                AbstractTransition trans(src, target);
                if (op_bucket.empty() || op_bucket.back() != trans)
                    op_bucket.push_back(trans);
            }
        }

        delete reducer;
        // dump();
    }
    */


void SymSMAS::build_atomic_abstractions(SymVariables *bdd_vars,
                                        bool is_unit_cost_, OperatorCost cost_type_,
                                        std::vector<SymSMAS *> &result) {
    assert(result.empty());
    std::cout << "Building atomic abstractions... " << std::endl;
    int var_count = global_simulation_task()->get_num_variables();

    // Step 1: Create the abstraction objects without transitions.
    for (int var_no = 0; var_no < var_count; var_no++)
        result.push_back(new SymSMAS(bdd_vars, is_unit_cost_, cost_type_, var_no));

    // Step 2: Add transitions.
    for (int op_no = 0; op_no < global_simulation_task()->get_num_operators(); op_no++) {
        OperatorID op(op_no);
        // There are no more spurious operators anymore (removed in preprocess)
        /*if(op->spurious){
          std::cout << "Skip " << op->get_name() << std::endl;
          continue;
          }*/
        const std::vector<Prevail> &prevs = get_prevails(op);
        for (auto prev: prevs) {
            int var = prev.var;
            int value = prev.prev;
            SymSMAS *abs = result[var];
            AbstractTransition trans(value, value);
            abs->transitions_by_op[op_no].push_back(trans);

            if (abs->relevant_operators.empty()
                || abs->relevant_operators.back() != op)
                abs->relevant_operators.push_back(op);
        }
        const std::vector<PrePost> &pre_posts = get_preposts(op);
        for (const auto &pre_post: pre_posts) {
            int var = pre_post.var;
            int pre_value = pre_post.pre;
            int post_value = pre_post.post;
            SymSMAS *abs = result[var];
            int pre_value_min, pre_value_max;
            if (pre_value == -1) {
                pre_value_min = 0;
                pre_value_max = global_simulation_task()->get_variable_domain_size(var);
            } else {
                pre_value_min = pre_value;
                pre_value_max = pre_value + 1;
            }
            for (int value = pre_value_min; value < pre_value_max; value++) {
                AbstractTransition trans(value, post_value);
                abs->transitions_by_op[op_no].push_back(trans);
            }
            if (abs->relevant_operators.empty()
                || abs->relevant_operators.back() != op)
                abs->relevant_operators.push_back(op);
        }
        for (auto abs: result) {
            if (abs->relevant_operators.empty()
                || abs->relevant_operators.back() != op)
                abs->irrelevant_operators.push_back(op);
        }
    }
}


void SymSMAS::apply_abstraction(std::vector<AbstractStateRefList> &collapsed_groups) {
    /* Note on how this method interacts with the distance information
       (init_distances and goal_distances): if no two states with
       different g or h values are combined by the abstraction (i.e.,
       if the abstraction is "f-preserving", then this method makes
       sure sure that distance information is preserved.

       This is important because one of the (indirect) callers of this
       method is the distance computation code, which uses it in a
       somewhat roundabout way to get rid of irrelevant and
       unreachable states. That caller will always give us an
       f-preserving abstraction.

       When called with a non-f-preserving abstraction, distance
       information is cleared as a side effect. In most cases we won't
       actually need it any more at this point anyway, so it is no
       great loss.

       Still, it might be good if we could find a way to perform the
       unreachability and relevance pruning that didn't introduce such
       tight coupling between the distance computation and abstraction
       code. It would probably also a good idea to do the
       unreachability/relevance pruning as early as possible, e.g.
       right after construction.
    */

    DEBUG_MSG(std::cout << tag() << "applying abstraction (" << size()
                        << " to " << collapsed_groups.size() << " states)" << std::endl;
              );

    typedef std::list<AbstractStateRef> Group;

    std::vector<int> abstraction_mapping(num_states, PRUNED_STATE);

    for (int group_no = 0; group_no < collapsed_groups.size(); group_no++) {
        Group &group = collapsed_groups[group_no];
        //	std::cout << "Looking at group: " << group.size() << std::endl;
        for (int state: group) {
            assert(abstraction_mapping[state] == PRUNED_STATE);
            abstraction_mapping[state] = group_no;
        }
    }
    DEBUG_MSG(std::cout << "step 1 done" << std::endl;
              );
    int new_num_states = collapsed_groups.size();
    std::vector<int> new_init_distances(new_num_states, PLUS_INFINITY);
    std::vector<int> new_goal_distances(new_num_states, PLUS_INFINITY);
    std::vector<bool> new_goal_states(new_num_states, false);

    std::vector<std::shared_ptr<SMASAbsState>> newAbsStates;
    newAbsStates.reserve(new_num_states);
    for (auto &ss: shrinkStates) {
        ss->marked = true;     //Mark all the shrink states
        //std::cout << "Marked: " << ss.get() << std::endl;
    }

    bool must_clear_distances = false;
    for (AbstractStateRef new_state = 0;
         new_state < collapsed_groups.size(); new_state++) {
        Group &group = collapsed_groups[new_state];
        assert(!group.empty());
        auto pos = group.begin();
        int &new_init_dist = new_init_distances[new_state];
        int &new_goal_dist = new_goal_distances[new_state];

        new_init_dist = init_distances[*pos];
        new_goal_dist = goal_distances[*pos];

        new_goal_states[new_state] = goal_states[*pos];

        ++pos;
        for (; pos != group.end(); ++pos) {
            if (init_distances[*pos] < new_init_dist) {
                must_clear_distances = true;
                new_init_dist = init_distances[*pos];
            }
            if (goal_distances[*pos] < new_goal_dist) {
                must_clear_distances = true;
                new_goal_dist = goal_distances[*pos];
            }
            if (goal_states[*pos])
                new_goal_states[new_state] = true;
        }

        if (group.size() > 1) {
            auto newss = std::make_shared<SMASShrinkState>(vars, absStates, group);
            shrinkStates.push_back(newss);
            newAbsStates.push_back(std::make_shared<SMASAbsState>(newss, vars));
        } else {
            int index = group.front();
            /*if(absStates[index]->shrinkState->marked){
          std::cout << "Keep: " << absStates[index]->shrinkState.get() << std::endl;
          }*/
            absStates[index]->shrinkState->marked = false;
            newAbsStates.push_back(absStates[index]);
        }
    }

    DEBUG_MSG(std::cout << "Shrink states before: " << shrinkStates.size() << std::endl;
              );

    //Clear shrink states that are not used anymore
    shrinkStates.erase(std::remove_if
                           (std::begin(shrinkStates), std::end(shrinkStates),
                           [](std::shared_ptr<SMASShrinkState> &ss) -> bool {
                               return ss->marked;
                           }), std::end(shrinkStates));

    DEBUG_MSG(std::cout << "Shrink states after: " << shrinkStates.size() << std::endl;
              );

    DEBUG_MSG(std::cout << "step 2, abstract states created done" << std::endl;
              );

    // Release memory.
    std::vector<int>().swap(init_distances);
    std::vector<int>().swap(goal_distances);
    std::vector<bool>().swap(goal_states);

    std::vector<std::vector<AbstractTransition>> new_transitions_by_op(
        transitions_by_op.size());
    for (int op_no = 0; op_no < transitions_by_op.size(); op_no++) {
        const std::vector<AbstractTransition> &transitions =
            transitions_by_op[op_no];
        std::vector<AbstractTransition> &new_transitions =
            new_transitions_by_op[op_no];
        new_transitions.reserve(transitions.size());
        for (const auto &trans: transitions) {
            int src = abstraction_mapping[trans.src];
            int target = abstraction_mapping[trans.target];
            if (src != PRUNED_STATE && target != PRUNED_STATE)
                new_transitions.emplace_back(src, target);
        }
    }
    std::vector<std::vector<AbstractTransition>>().swap(transitions_by_op);

    DEBUG_MSG(std::cout << "step 3, new transitions done" << std::endl;
              );

    num_states = new_num_states;
    transitions_by_op.swap(new_transitions_by_op);
    init_distances.swap(new_init_distances);
    goal_distances.swap(new_goal_distances);
    goal_states.swap(new_goal_states);
    init_state = abstraction_mapping[init_state];
    if (init_state == PRUNED_STATE)
        std::cout << tag() << "initial state pruned; task unsolvable" << std::endl;


    std::vector<std::shared_ptr<SMASAbsState>>().swap(absStates);
    absStates.swap(newAbsStates);

    DEBUG_MSG(std::cout << "Removed empty shrink states " << std::endl;
              );

    if (must_clear_distances) {
        f_preserved = false;
        std::cout << tag() << "simplification was not f-preserving!" << std::endl;
        clear_distances();
    }

    //    printBDDs();

    // for(int i = 0; i < shrinkStates.size(); i++){
    //   std::cout << "pointer: " << shrinkStates[i].get() << std::endl;
    // }

    DEBUG_MSG(std::cout << "number of shrink equivalences: " << shrinkStates.size() << " and abstract states: "
                        << absStates.size() << std::endl;
              );
    /*for(int i = 0; i < shrinkStates.size(); i++){
      std::cout << "pointer3: " << shrinkStates[i].get() << std::endl;
      }*/
}


bool SymSMAS::is_solvable() const {
    return init_state != PRUNED_STATE;
}

int SymSMAS::memory_estimate() const {
    int result = sizeof(SymSMAS);
    result += sizeof(OperatorID) * relevant_operators.capacity();
    result += sizeof(OperatorID) * irrelevant_operators.capacity();
    result += sizeof(std::vector<AbstractTransition>)
        * transitions_by_op.capacity();
    for (const auto &i: transitions_by_op)
        result += sizeof(AbstractTransition) * i.capacity();
    result += sizeof(int) * init_distances.capacity();
    result += sizeof(int) * goal_distances.capacity();
    result += sizeof(bool) * goal_states.capacity();
    //lalala    result += bddsSize;
    return result;
}

void SymSMAS::release_memory() {
    DEBUG_MSG(std::cout << "Release memory of: " << *this << std::endl;
              );
    std::vector<OperatorID>().swap(relevant_operators);
    std::vector<OperatorID>().swap(irrelevant_operators);
    //std::vector<std::vector<AbstractTransition> >().swap(transitions_by_op);
}

int SymSMAS::total_transitions() const {
    int total = 0;
    for (const auto &i: transitions_by_op)
        total += i.size();
    return total;
}


int SymSMAS::count_spurious_states() const {
    int num = 0;
    for (const auto &s: absStates) {
        if (s->is_spurious())
            num++;
    }
    return num;
}

int SymSMAS::unique_unlabeled_transitions() const {
    std::vector<AbstractTransition> unique_transitions;
    for (const auto &trans: transitions_by_op) {
        unique_transitions.insert(unique_transitions.end(), trans.begin(), trans.end());
    }
    std::sort(unique_transitions.begin(), unique_transitions.end());
    return std::unique(unique_transitions.begin(), unique_transitions.end()) - unique_transitions.begin();
}

void SymSMAS::statistics(bool include_expensive_statistics) const {
    int memory = memory_estimate();
    peak_memory = std::max(peak_memory, memory);
    std::cout << tag() << size() << " states, ";
    if (include_expensive_statistics)
        std::cout << count_spurious_states() << " states detected spurious, " << unique_unlabeled_transitions();
    else
        std::cout << "???";
    std::cout << "/" << total_transitions() << " arcs, " << memory << " bytes"
              << std::endl;
    std::cout << tag();
    if (max_h == DISTANCE_UNKNOWN) {
        std::cout << "distances not computed";
    } else if (is_solvable()) {
        std::cout << "init h=" << goal_distances[init_state] << ", max f=" << max_f
                  << ", max g=" << max_g << ", max h=" << max_h;
    } else {
        std::cout << "abstraction is unsolvable";
    }
    std::cout << " [t=" << utils::g_timer << "]" << std::endl;
}

int SymSMAS::get_peak_memory_estimate() const {
    return peak_memory;
}

/*bool SymSMAS::is_in_absVars(int var) const {
  return absVars.count(var) != 0;
  }*/

void SymSMAS::dump() const {
    std::cout << "digraph abstract_transition_graph";
    for (auto av: absVars)
        std::cout << "_" << av;
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
    assert(transitions_by_op.size() == global_simulation_task()->get_num_operators());
    for (int op_no = 0; op_no < global_simulation_task()->get_num_operators(); op_no++) {
        const std::vector<AbstractTransition> &trans = transitions_by_op[op_no];
        for (auto t: trans) {
            int src = t.src;
            int target = t.target;
            std::cout << "    node" << src << " -> node" << target << " [label = o_" << op_no << "];" << std::endl;
        }
    }
    std::cout << "}" << std::endl;
}


BDD SymSMAS::shrinkExists(const BDD &from, int maxNodes) const {
    if (shrinkStates.empty()) {
        return from;
    }
    //  std::cout << "Shrink" << std::endl;
    utils::Timer sh_timer;
    DEBUG_MSG(std::cout << " Shrinking: " << shrinkStates.size() << " ";
              );                                                               //absVarsCube.print(1, 1););
    std::vector<BDD> res;
    res.reserve(shrinkStates.size());
    for (const auto &ss: shrinkStates) {
        /*if(sh_timer()*1000 > maxTime) {
          std::cout << "time exceeded: " << sh_timer() << " > " << maxTime << std::endl;
          throw BDDError();
          }*/
        //std::cout << "Processing " << ss.get() << std::endl;
        //    DEBUG_MSG(std::cout << "abs: " << *(ss.get()) << " from: " << from.nodeCount()<< "   ";);
        BDD aux = ss->shrinkExists(from, maxNodes);

        //    DEBUG_MSG(std::cout << " res: " << aux.nodeCount() <<
        //	      " time: " << sh_timer() << " while is <= " << maxTime << std::endl;);

        if (!aux.IsZero()) {
            res.push_back(aux);
            //  std::cout << " res: " << res.back().nodeCount() <<
            // " time: " << sh_timer() << std::endl;
        }
    }

    if (res.empty()) {
        DEBUG_MSG(std::cout << "Shrink to zeroBDD took " << sh_timer << "s. " << std::endl;
                  );
        return vars->zeroBDD();
    }
    if (res.size() > 1) {
        merge(res, mergeOrBDD /*, maxTime - sh_timer()*1000*/, maxNodes);
        if (res.size() > 1) {
            DEBUG_MSG(std::cout << "could not merge. Left: " << res.size() << std::endl;
                      );
            throw BDDError();
        }
    }

    std::cout << "Shrink to " << res[0].nodeCount() << " took " << sh_timer << "s. " << std::endl;
    return res[0];
}

BDD SymSMAS::shrinkForall(const BDD &from, int maxNodes) const {
    if (shrinkStates.empty()) {
        return from;
    }
    std::vector<BDD> res;
    res.reserve(shrinkStates.size());
    utils::Timer sh_timer;
    for (const auto &ss: shrinkStates) {
        /*if(sh_timer()*1000 > maxTime) {
          throw BDDError();
          }*/
        //std::cout << "AbsState " << i << ": "; shrinkStates[i].print(1, 2);
        //    std::cout << "And abstract: " << from.AndAbstract(shrinkStates[i], absVarsCube).print(1, 2);
        BDD aux = ss->shrinkForall(from, maxNodes);
        if (!aux.IsZero()) {
            res.push_back(aux);
        }
    }

    if (res.empty()) {
        std::cout << "Shrink to zeroBDD took " << sh_timer << "s. " << std::endl;
        return vars->zeroBDD();
    }

    if (res.size() > 1) {
        merge(res, mergeOrBDD, maxNodes);
        if (res.size() > 1) {
            std::cout << "could not merge: " << res.size() << std::endl;
            throw BDDError();
        }
    }
    std::cout << "Shrink took " << sh_timer << ". " << std::flush;
    return res[0];
}


BDD SymSMAS::getInitialState() const {
    return shrinkExists(vars->getStateBDD(global_simulation_task()->get_initial_state_values()), PLUS_INFINITY);
}

BDD SymSMAS::getGoal() const {
    std::vector<std::pair<int, int>> abstract_goal;
    for (int i = 0; i < global_simulation_task()->get_num_goals(); ++i) {
        auto goal = global_simulation_task()->get_goal_fact(i);
        abstract_goal.emplace_back(goal.var, goal.value);
    }
    return shrinkExists(vars->getPartialStateBDD(abstract_goal), PLUS_INFINITY);
}

BDD SymSMAS::shrinkTBDD(const BDD & /*tBDD*/, int /*maxNodes*/) const {
    std::cout << "Shrink TBD not implemented in SMAS" << std::endl;
    exit(-1);
}

void SymSMAS::print(std::ostream &os, bool /*fullInfo*/) const {
    os << "SMAS " << fullVars.size() << "," << absVars.size();
}


//void SymSMAS::getTransitions(const std::map<int, std::vector<SymTransition>> & /*individualTRs*/,
//                             std::map<int, std::vector<SymTransition>> & /*trs*/,
//                             int /*maxTime*/, int/* maxNodes*/) const {
// for(const auto & indTRsCost : individualTRs){
//   int cost = indTRsCost.first;
//   for(const auto & indTR : indTRsCost.second){
//      SymTransition absTransition = SymTransition(indTR);
//      if(absTransition.shrink(*this, maxNodes)){
//        int id_op = get_op_index(*(indTR.getOps().begin()));
//        for(const AbstractTransition & tr : transitions_by_op[id_op]){
//          std::cout << cost << tr.src << std::endl;
//          /*BDD bddSrc = absStates[tr.src].getPreBDD();
//          BDD bddTarget = absStates[tr.target].getEffBDD();
//          SymTransition newTR {absTransition};
//          newTR.setMaSAbstraction(bddSrc, bddTarget);
//          trs[cost].push_back(newTR);*/
//        }
//      }else{
//        //  failedToShrink[cost].push_back(absTransition);
//      }
//   }
// }
//}


ADD SymSMAS::getExplicitHeuristicADD(bool fw) {
    std::cout << "Getting final explicit heuristic from " << *this << std::endl;
    if (goal_distances.empty() || init_distances.empty())
        compute_distances();
    ADD h = vars->getADD(-1);
    for (int i = 0; i < num_states; i++) {
        int distance = 1 + (fw ? init_distances[i] : goal_distances[i]);
        h += absStates[i]->getBDD().Add() * vars->getADD(distance);
    }
    std::cout << "ADD Heuristic size: " << h.nodeCount() << std::endl;
    return h;
}

void SymSMAS::getExplicitHeuristicBDD(bool fw, std::map<int, BDD> &res) {
    if (goal_distances.empty() || init_distances.empty())
        compute_distances();

    std::map<int, std::vector<BDD>> aux;
    DEBUG_MSG(std::cout << "Getting explicit heuristic with num_states=" << num_states << std::endl;
              );
    for (int i = 0; i < num_states; i++) {
        int h = (fw ? init_distances[i] : goal_distances[i]);
        if (h != PRUNED_STATE) {
            aux[h].push_back(absStates[i]->getBDD());
        }
    }

    std::vector<BDD> reached;
    for (auto &a: aux) {
        DEBUG_MSG(std::cout << "Merging " << a.second.size() << " bdds with h=" << a.first << std::endl;
                  );
        merge(a.second, mergeOrBDD, PLUS_INFINITY);
        res[a.first] = a.second[0];
        reached.push_back(a.second[0]);
    }
    DEBUG_MSG(std::cout << "Merging " << reached.size() << " bdds to get not reached" << std::endl;
              );
    merge(reached, mergeOrBDD, PLUS_INFINITY);
    BDD not_reached = !(reached[0]);
    if (!not_reached.IsZero()) {
        res[-1] = not_reached;
    }
}
}
