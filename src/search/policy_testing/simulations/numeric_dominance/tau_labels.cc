#include "tau_labels.h"

#include <iostream>

#include "int_epsilon.h"
#include "breadth_first_search.h"
#include "dijkstra_search_epsilon.h"
#include "../../../plugins/plugin.h"

namespace simulations {
constexpr const int TAU_IN_ALL = -1;
constexpr const int TAU_IN_NONE = -2;

template<typename T>
TauLabels<T>::TauLabels(const std::vector<LabelledTransitionSystem *> &lts,
                        const LabelMap &label_map, bool self_loops) {
    tau_labels.resize(lts.size());
    tau_label_cost.resize(lts.size());

    int num_labels = label_map.get_num_labels();
    original_cost.resize(num_labels);
    label_relevant_for.resize(num_labels);

    num_tau_labels_for_some = 0;
    num_tau_labels_for_all = 0;

    for (int l = 0; l < num_labels; l++) {
        original_cost[l] = epsilon_if_zero<T>(label_map.get_cost(l));

        assert(original_cost[l] != T(0));

        int transition_system_relevant = TAU_IN_ALL;
        for (int lts_id = 0; lts_id < lts.size(); ++lts_id) {
            if ((self_loops && !lts[lts_id]->is_self_loop_everywhere_label(l)) ||
                (!self_loops && lts[lts_id]->is_relevant_label(l))) {
                if (transition_system_relevant == TAU_IN_ALL) {
                    transition_system_relevant = lts_id;
                } else {
                    transition_system_relevant = TAU_IN_NONE;
                    break;
                }
            }
        }
        //TODO: check why there are labels irrelevant everywhere
        // assert(transition_system_relevant != -1);
        if (transition_system_relevant >= 0) {
            num_tau_labels_for_some++;
            tau_labels[transition_system_relevant].push_back(l);
        }
        label_relevant_for[l] = transition_system_relevant;
    }

    std::cout << "Computed tau labels as self-loops everywhere: " << num_tau_labels_for_all << " : " <<
        num_tau_labels_for_some << " / " << num_labels << "\n";
}


template<typename T> std::set<int>
TauLabels<T>::add_recursive_tau_labels(const std::vector<LabelledTransitionSystem *> &lts,
                                       const std::vector<std::unique_ptr<TauDistances<T>>> &tau_distances) {
    int num_labels = original_cost.size();
    std::set<int> check_distances_of;

    assert(lts.size() == tau_distances.size());

    for (int l = 0; l < num_labels; l++) {
        T total_tau_cost = 0;
        int transition_system_relevant = TAU_IN_ALL;
        for (int lts_id = 0; lts_id < lts.size(); ++lts_id) {
            assert(tau_distances[lts_id]);
            if (lts[lts_id]->is_relevant_label(l) && !tau_distances[lts_id]->is_fully_invertible()) {
                if (transition_system_relevant == TAU_IN_ALL) {
                    transition_system_relevant = lts_id;
                } else {
                    transition_system_relevant = TAU_IN_NONE;
                    break;
                }
            } else if (lts[lts_id]->is_relevant_label(l)) {
                total_tau_cost += tau_distances[lts_id]->get_cost_fully_invertible();
            }
        }

        if (transition_system_relevant == TAU_IN_ALL && label_relevant_for[l] != TAU_IN_ALL) {
            //This label is a tau label in all transition systems
            for (int lts_id = 0; lts_id < lts.size(); ++lts_id) {
                if (lts[lts_id]->is_relevant_label(l) && label_relevant_for[l] != lts_id) {
                    tau_labels[lts_id].push_back(l);
                    check_distances_of.insert(lts_id);

                    set_tau_cost(lts_id, l, total_tau_cost -
                                 tau_distances[lts_id]->get_cost_fully_invertible());
                }
            }

            if (label_relevant_for[l] == TAU_IN_NONE) {
                num_tau_labels_for_some++;
            }
            num_tau_labels_for_all++;
        } else if (transition_system_relevant >= 0 && label_relevant_for[l] == TAU_IN_NONE) {
            tau_labels[transition_system_relevant].push_back(l);
            set_tau_cost(transition_system_relevant, l, total_tau_cost);
            check_distances_of.insert(transition_system_relevant);
            num_tau_labels_for_some++;
        }

        label_relevant_for[l] = transition_system_relevant;
    }

    std::cout << "Computed tau labels recursive: " << num_tau_labels_for_all << " : " << num_tau_labels_for_some <<
        " / " << num_labels << "\n";
    return check_distances_of;
}


template<typename T>
bool TauDistances<T>::precompute(const TauLabels<T> &tau_labels,
                                 const LabelledTransitionSystem *lts,
                                 int lts_id, bool only_reachability) {
    if (!distances_with_tau.empty() && num_tau_labels == tau_labels.size(lts_id)) {
        return false;
    }

    // //There may be no new tau labels
    // assert(num_tau_labels >= tau_labels.size(lts_id));

    num_tau_labels = tau_labels.size(lts_id);
    int num_states = lts->size();
    distances_with_tau.resize(num_states);
    reachable_with_tau.resize(num_states);
    // max_cost_reach_with_tau.resize(num_states, 0);
    // max_cost_reach_from_with_tau.resize(num_states, 0);

    const auto copy_distances = distances_with_tau;

    if (only_reachability) {
        //Create copy of the graph only with tau transitions
        std::vector<std::vector<int>> tau_graph(num_states);
        for (int label_no : tau_labels.get_tau_labels(lts_id)) {
            for (const auto &trans : lts->get_transitions_label(label_no)) {
                if (trans.src != trans.target) {
                    tau_graph[trans.src].push_back(trans.target);
                }
            }
        }

        for (int s = 0; s < num_states; ++s) {
            auto &distances = distances_with_tau [s];
            distances.resize(num_states);
            std::fill(distances.begin(), distances.end(), std::numeric_limits<int>::max());
            distances[s] = 0;
            reachable_with_tau[s].clear();

            //Perform Dijkstra search from s
            breadth_first_search_reachability_distances_one(tau_graph, s, distances,
                                                            reachable_with_tau[s]);

            //cout << "BFS finished " << reachable_with_tau[s].size() << endl;
        }
    } else {
        //Create copy of the graph only with tau transitions
        std::vector<std::vector<std::pair<int, T>>> tau_graph(num_states);
        for (int label_no : tau_labels.get_tau_labels(lts_id)) {
            for (const auto &trans : lts->get_transitions_label(label_no)) {
                if (trans.src != trans.target) {
                    tau_graph[trans.src].push_back(std::make_pair(trans.target,
                                                                  tau_labels.get_cost(lts_id, label_no)));
                }
            }
        }

        for (int s = 0; s < num_states; ++s) {
            //Perform Dijkstra search from s
            auto &distances = distances_with_tau [s];
            distances.resize(num_states);
            reachable_with_tau[s].clear();
            std::fill(distances.begin(), distances.end(), std::numeric_limits<int>::max());
            distances[s] = 0;
            dijkstra_search_epsilon(tau_graph, s, distances, reachable_with_tau[s]);
        }
    }

    goal_distances_with_tau.resize(num_states);
    for (int s = 0; s < num_states; ++s) {
        goal_distances_with_tau[s] = std::numeric_limits<int>::max();
        for (int t = 0; t < num_states; t++) {
            if (lts->is_goal(t)) {
                goal_distances_with_tau[s] = std::min(goal_distances_with_tau[s], distances_with_tau[s][t]);
            }
        }
    }


    cost_fully_invertible = 0;

    for (int s = 0; s < num_states; ++s) {
        if (reachable_with_tau[s].size() < num_states) {
            cost_fully_invertible = std::numeric_limits<int>::max();
        } else {
            for (int sp = 0; sp < num_states; ++sp) {
                cost_fully_invertible = std::max(cost_fully_invertible,
                                                 distances_with_tau [s][sp] + distances_with_tau [s][sp]);
                //      max_cost_reach_with_tau [sp] = max(max_cost_reach_with_tau [sp], distances[sp]);
                //      max_cost_reach_from_with_tau [s] = max(max_cost_reach_from_with_tau [s], distances[sp]);
            }
        }
    }

    if (cost_fully_invertible < std::numeric_limits<int>::max()) {
        std::cout << "Fully invertible: " << lts_id << " with cost " << cost_fully_invertible << std::endl;
    }

    if (copy_distances != distances_with_tau) {
        id++;
        return true;
    }
    return false;
}

template<> int TauDistances<int>::get_cost_fully_invertible() const {
    return cost_fully_invertible;
}

template<> IntEpsilon TauDistances<IntEpsilon>::get_cost_fully_invertible() const {
    return cost_fully_invertible.get_value() + 1;
}

template<typename T>
void TauLabelManager<T>::initialize(const std::vector<LabelledTransitionSystem *> &lts,
                                    const LabelMap &label_map) {
    tau_labels = make_unique<TauLabels<T>> (lts, label_map, self_loops);

    tau_distances.resize(lts.size());

    //First precompute distances
    for (int lts_id = 0; lts_id < lts.size(); ++lts_id) {
        tau_distances[lts_id] = std::make_unique<TauDistances<T>>();
        tau_distances[lts_id]->precompute(*tau_labels, lts[lts_id], lts_id, only_reachability);
    }

    if (recursive) {
        bool changes = true;
        while (changes) {
            changes = false;
            for (int ts : tau_labels->add_recursive_tau_labels(lts, tau_distances)) {
                changes |= tau_distances[ts]->precompute(*tau_labels, lts[ts], ts, false);
            }
        }
    }
}



template<typename T>
bool TauLabelManager<T>::add_noop_dominance_tau_labels(const std::vector<LabelledTransitionSystem *> &lts,
                                                       const NumericLabelRelation<T> &label_dominance) {
    if (!noop_dominance) {
        return false;
    }

    bool some_changes = false;
    for (int ts : tau_labels->add_noop_dominance_tau_labels(label_dominance)) {
        some_changes |= tau_distances[ts]->precompute(*tau_labels, lts[ts], ts, only_reachability && !recursive);
    }

    if (recursive) {
        bool changes = true;
        while (changes) {
            changes = false;
            for (int ts : tau_labels->add_recursive_tau_labels(lts, tau_distances)) {
                changes |= tau_distances[ts]->precompute(*tau_labels, lts[ts], ts, false);
            }
            some_changes |= changes;
        }
    }

    return some_changes;
}

template<typename T>
std::set<int> TauLabels<T>::add_noop_dominance_tau_labels(const NumericLabelRelation<T> &label_dominance) {
    std::set<int> ts_with_new_tau_labels;
    int num_ltss = tau_labels.size();

    int num_labels = original_cost.size();

    std::cout << "Compute tau labels with noop dominance" << "\n";
    for (int l = 0; l < num_labels; l++) {
        if (label_relevant_for[l] == TAU_IN_ALL)
            continue;
        if (label_dominance.dominates_noop_in_all(l)) {
            if (label_relevant_for[l] == TAU_IN_NONE) {
                for (int lts_id = 0; lts_id < num_ltss; ++lts_id) {
                    tau_labels[lts_id].push_back(l);
                    ts_with_new_tau_labels.insert(lts_id);
                    if (label_dominance.q_dominates_noop(l, lts_id) < 0) {
                        set_tau_cost(lts_id, l, -label_dominance.q_dominates_noop(l, lts_id));
                    }
                }
                num_tau_labels_for_some++;
            } else if (label_relevant_for[l] >= 0) {
                // TODO (Jan) Why add tau labels for l and not for all LTS but l
                int lts_id = label_relevant_for[l];
                // TODO (Jan) confirm check (added)
                assert(std::find(tau_labels[lts_id].begin(), tau_labels[lts_id].end(), l) == tau_labels[lts_id].end());
                tau_labels[lts_id].push_back(l);
                ts_with_new_tau_labels.insert(lts_id);
                if (label_dominance.q_dominates_noop(l, lts_id) < 0) {
                    set_tau_cost(lts_id, l, -label_dominance.q_dominates_noop(l, lts_id));
                }
            }
            num_tau_labels_for_all++;
            std::cout << global_simulation_task()->get_operator_name(l, false) << " is tau for all " << std::endl;

            label_relevant_for[l] = TAU_IN_ALL;
        } else if (label_dominance.dominates_noop_in_all_but_one(l)) {
            int lts_id = label_dominance.get_dominates_noop_in(l);
            // l dominates noop in all LTS except for lts_id
            if (label_relevant_for[l] == TAU_IN_NONE) {
                num_tau_labels_for_some++;
                tau_labels[lts_id].push_back(l);
                ts_with_new_tau_labels.insert(lts_id);
                if (label_dominance.q_dominates_noop(l, lts_id) < 0) {
                    set_tau_cost(lts_id, l, -label_dominance.q_dominates_noop(l, lts_id));
                }
                // TODO (Jan) confirm
                label_relevant_for[l] = lts_id;
            } else {
                // TODO (Jan) assertion does not always hold (see visitall2)
                // make sure label_relevant for is not changed
                // assert(label_relevant_for[l] == lts_id);
            }
            // TODO (Jan) moved in if branch
            // label_relevant_for[l] = lts_id;
        }
    }

    std::cout << "Computed tau labels noop: " << num_tau_labels_for_all << " : " << num_tau_labels_for_some << " / " <<
        num_labels << "\n";

    return ts_with_new_tau_labels;
}




template<typename T>
TauLabelManager<T>::TauLabelManager(const plugins::Options &opts,
                                    bool only_reachability_) : \
    only_reachability(only_reachability_),
    self_loops(opts.get<bool>("tau_labels_self_loops")),
    recursive(opts.get<bool>("tau_labels_recursive")),
    noop_dominance(opts.get<bool>("tau_labels_noop")) {
    // compute_tau_labels_with_noop_dominance(opts.get<bool>("compute_tau_labels_with_noop_dominance")),
    // tau_label_dominance(opts.get<bool>("tau_label_dominance")),
}

template<typename T>
void TauLabelManager<T>::add_options_to_feature(plugins::Feature &feature) {
    feature.add_option<bool>("tau_labels_recursive",
                             "Use stronger notion of tau labels based on self loops everywhere",
                             "true");

    feature.add_option<bool>("tau_labels_self_loops",
                             "Use stronger notion of tau labels based on self loops everywhere",
                             "true");

    feature.add_option<bool>("tau_labels_noop",
                             "Use stronger notion of tau labels based on noop",
                             "false");
}


template<typename T>
void TauLabelManager<T>::print_config() const {
    std::cout << "Tau labels self_loops: " << self_loops << std::endl;
    std::cout << "Tau labels recursive: " << recursive << std::endl;
    std::cout << "Tau labels noop: " << noop_dominance << std::endl;
}

template class TauLabelManager<int>;
template class TauLabelManager<IntEpsilon>;
}
