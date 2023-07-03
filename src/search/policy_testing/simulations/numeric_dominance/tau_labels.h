#pragma once

#include <vector>
#include <memory>

#include <cassert>
#include "numeric_label_relation.h"

namespace options {
class OptionParser;
class Options;
}

namespace simulations {
class LabelledTransitionSystem;
class LabelMap;
class LabelDominance;

template<typename T>
class TauDistances;



template<typename T>
class TauLabels {
    int num_tau_labels_for_some, num_tau_labels_for_all;
    std::vector<T> original_cost;

    //The cost of applying a tau label may be higher than the cost of the label since it
    //may require executing other tau label moves in different transition systems.
    std::vector<int> label_relevant_for;
    std::vector<std::vector<T>> tau_label_cost;

    std::vector<std::vector<int>> tau_labels;

public:
    // TauLabels(int num_ltss) : tau_labels(num_ltss) {}
    TauLabels(const std::vector<LabelledTransitionSystem *> &lts, const LabelMap &label_map, bool self_loops);

    std::set<int>
    add_recursive_tau_labels(const std::vector<LabelledTransitionSystem *> &lts,
                             const std::vector<std::unique_ptr<TauDistances<T>>> &tau_distances);

    [[nodiscard]] bool empty() const {
        return num_tau_labels_for_some == 0;
    }

    std::set<int> add_noop_dominance_tau_labels(const NumericLabelRelation<T> &label_dominance);

    [[nodiscard]] int size(int lts) const {
        return tau_labels[lts].size();
    }

    [[nodiscard]] const std::vector<int> &get_tau_labels(int lts) const {
        return tau_labels[lts];
    }


    T get_cost(int lts, int label) const {
        if (tau_label_cost[lts].empty()) {
            return original_cost[label];
        }
        return tau_label_cost[lts][label] + original_cost[label];
    }

    void set_tau_cost(int lts, int label, T tau_cost) {
        if (tau_label_cost[lts].empty()) {
            tau_label_cost[lts].resize(original_cost.size(), 0);
        }
        assert(tau_cost >= 0);
        tau_label_cost[lts][label] = tau_cost;
    }


    std::vector<std::vector<int>> &get_data() {
        return tau_labels;
    }

    template<typename F>
    void erase(int lts_id, F lambda) {
        erase_if(tau_labels[lts_id], lambda);
    }
};

template<typename T>
class TauDistances {
    int id;
    int num_tau_labels;
    std::vector<std::vector<T>> distances_with_tau;
    std::vector<T> goal_distances_with_tau;
    std::vector<std::vector<int>> reachable_with_tau;
    //std::vector<int> max_cost_reach_from_with_tau, max_cost_reach_with_tau;
    T cost_fully_invertible;
    // List of states for which distances_with_tau is not infinity
public:
    TauDistances() :
        id(0), num_tau_labels(0), cost_fully_invertible(std::numeric_limits<int>::max()) {
    }

    bool precompute(const TauLabels<T> &tau_labels, const LabelledTransitionSystem *lts, int lts_id,
                    bool only_reachability);

    [[nodiscard]] bool empty() const {
        return num_tau_labels == 0;
    }

    [[nodiscard]] int get_id() const {
        return id;
    }

    inline T minus_shortest_path(int from, int to) const {
        assert(from < distances_with_tau.size());
        assert(to < distances_with_tau[from].size());
        assert((from != to && distances_with_tau[from][to] > 0) ||
               (from == to && distances_with_tau[from][to] == 0));
        return distances_with_tau[from][to] == std::numeric_limits<int>::max()
                   ? std::numeric_limits<int>::lowest()
                   : -distances_with_tau[from][to];
    }

    inline T shortest_path(int from, int to) const {
        assert(from < distances_with_tau.size());
        assert(to < distances_with_tau[from].size());
        assert((from != to && distances_with_tau[from][to] > 0) ||
               (from == to && distances_with_tau[from][to] == 0));

        return distances_with_tau[from][to];
    }

    T get_goal_distance(int s) const {
        return goal_distances_with_tau[s];
    }

    [[nodiscard]] const std::vector<int> &states_reachable_from(int s) const {
        return reachable_with_tau[s];
    }

    [[nodiscard]] bool is_fully_invertible() const {
        return cost_fully_invertible < std::numeric_limits<int>::max();
    }

    T get_cost_fully_invertible() const;
};

template<typename T>
class TauLabelManager {
    const bool only_reachability;

    const bool self_loops;
    const bool recursive;
    const bool noop_dominance;

    std::unique_ptr<TauLabels<T>> tau_labels;
    std::vector<std::unique_ptr<TauDistances<T>>> tau_distances;
public:
    TauLabelManager (const Options &opts, bool only_reachability);

    void initialize(const std::vector<LabelledTransitionSystem *> &lts, const LabelMap &label_map);
    //void initialize();

    bool add_noop_dominance_tau_labels(const std::vector<LabelledTransitionSystem *> &lts,
                                       const NumericLabelRelation<T> &label_dominance);

    const TauDistances<T> &get_tau_distances(int lts_id) const {
        assert(tau_distances[lts_id]);
        return *(tau_distances[lts_id]);
    }

    TauLabels<T> &get_tau_labels() {
        assert(tau_labels);
        return *tau_labels;
    }

    void print_config() const;

    [[nodiscard]] bool has_tau_labels() const {
        return !tau_labels->empty();
    }

    static void add_options_to_parser(OptionParser &parser);
};
}
