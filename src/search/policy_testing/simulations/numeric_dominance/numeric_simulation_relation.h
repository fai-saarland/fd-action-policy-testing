#pragma once

#include <utility>
#include <vector>
#include <string>
#include <iostream>
#include "numeric_label_relation.h"
#include "int_epsilon.h"
#include "tau_labels.h"

namespace simulations {
class Labels;
class Abstraction;
class CompositeAbstraction;
class LabelledTransitionSystem;
class LTSTransition;

class StrippedNumericSimulationRelation {
    std::unique_ptr<StrippedAbstraction> abs;
    std::vector<std::vector<int>> relation;

    [[nodiscard]] inline int q_simulates(unsigned int s, unsigned int t) const {
        assert(s < relation.size());
        assert(t < relation[s].size());
        assert(s != t || relation[s][t] == 0);
        return relation[s][t];
    }

public:

    StrippedNumericSimulationRelation() = default;
    StrippedNumericSimulationRelation(std::unique_ptr<StrippedAbstraction> &&abs,
                                      std::vector<std::vector<int>> relation) :
        abs(std::move(abs)), relation(std::move(relation)) {}

    [[nodiscard]] int q_simulates(const State &t, const State &s) const {
        int tid = abs->get_abstract_state(t);
        int sid = abs->get_abstract_state(s);
        if (sid == -1 || tid == -1) {
            return MINUS_INFINITY;
        }
        return q_simulates(tid, sid);
    }

    [[nodiscard]] int q_simulates(const std::vector<int> &t, const std::vector<int> &s) const {
        int tid = abs->get_abstract_state(t);
        int sid = abs->get_abstract_state(s);
        if (sid == -1 || tid == -1) {
            return MINUS_INFINITY;
        }
        return q_simulates(tid, sid);
    }

    [[nodiscard]] int atomic_q_simulates(int t, int s) const {
        int tid = abs->get_atomic_abstract_state(t);
        int sid = abs->get_atomic_abstract_state(s);
        if (sid == -1 || tid == -1) {
            return MINUS_INFINITY;
        }
        return q_simulates(tid, sid);
    }

    /// returns the minimal negative finite entry of the relation table or 0 if no such entry exists
    [[nodiscard]] int get_min_finite_entry() const {
        int result = 0;
        for (unsigned int i = 0; i < relation.size(); ++i) {
            for (unsigned int j = 0; j < relation.size(); ++j) {
                if (i == j)
                    continue;
                const int entry = relation[i][j];
                if (entry != MINUS_INFINITY) {
                    result = std::min(entry, result);
                }
            }
        }
        return result;
    }

    bool operator==(const StrippedNumericSimulationRelation &other) const {
        assert(abs);
        assert(other.abs);
        return *abs == *other.abs && relation == other.relation;
    }

    friend class boost::serialization::access;
    template<typename Archive>
    void serialize(Archive &ar, const unsigned int /*version*/) {
        ar &abs;
        ar &relation;
    }
};

template<typename T>
class NumericSimulationRelation {
protected:
    const Abstraction *abs;
    const int truncate_value;

    std::shared_ptr<TauLabelManager<T>> tau_labels;
    int tau_distances_id{};

    std::vector<std::vector<T>> relation;

    T max_relation_value;

    std::vector<std::pair<int, int>> entries_with_positive_dominance;
    /*std::vector<std::vector<bool>> is_relation_stable; */

    bool cancelled;

    T compare_noop(int lts_id, int tr_s_target, int tr_s_label, int t,
                   T tau_distance,
                   const NumericLabelRelation<T> &label_dominance) const;


    T compare_transitions(int lts_id, int tr_s_target, int tr_s_label,
                          int tr_t_target, int tr_t_label,
                          T tau_distance,
                          const NumericLabelRelation<T> &label_dominance) const;

    int update_pair(int lts_id, const LabelledTransitionSystem *lts,
                    const NumericLabelRelation<T> &label_dominance,
                    const TauDistances<T> &tau_distances,
                    int s, int t);

public:

    NumericSimulationRelation(Abstraction *_abs, int truncate_value,
                              std::shared_ptr<TauLabelManager<T>> tau_labels_mgr);

    void init_goal_respecting();

    int update(int lts_id, const LabelledTransitionSystem *lts,
               const NumericLabelRelation<T> &label_dominance,
               int max_time);


    void cancel_simulation_computation(int lts_id, const LabelledTransitionSystem *lts);

    [[nodiscard]] bool pruned(const State &state) const;

    T q_simulates(const State &t, const State &s) const;

    T q_simulates(const std::vector<int> &t, const std::vector<int> &s) const;

    /**
     * Version of q_simulates for atomic abstractions, where @param t and @param s are the values of the state
     * variable used in the atomic abstraction (and not necessarily the internal ids used by the abstraction).
     * The method converts t and s to the respective abstraction ids and returns the simulation value.
     * @warning only usable when simulation relation is based on an atomic abstraction.
     */
    T atomic_q_simulates(int t, int s) const;

    [[nodiscard]] int get_abstract_state_id(const State &t) const;

    [[nodiscard]] int get_abstract_state_id(const std::vector<int> &t) const;

    [[nodiscard]] inline bool simulates(unsigned int s, unsigned int t) const {
        return relation[s][t] >= 0;
    }

    [[nodiscard]] inline bool may_simulate(unsigned int s, unsigned int t) const {
        assert(s < relation.size());
        assert(t < relation[s].size());
        return relation[s][t] > MINUS_INFINITY;
    }

    inline T q_simulates(unsigned int s, unsigned int t) const {
        assert(s < relation.size());
        assert(t < relation[s].size());
        assert(s != t || relation[s][t] == 0);
        return relation[s][t];
    }

    /*
    [[nodiscard]] inline bool strictly_simulates(unsigned int s, unsigned int t) const {
        return relation[s][t] >= 0 && relation[t][s] < 0;
    }
     */


    /*
    [[nodiscard]] inline bool positively_simulates(unsigned int s, unsigned int t) const {
        assert(s < relation.size());
        assert(t < relation[s].size());
        assert(s != t || relation[s][t] == 0);
        return relation[s][t] >= 0;
    }
     */

    /*
    [[nodiscard]] inline bool similar(int s, int t) const {
        return simulates(s, t) && simulates(t, s);
    }
     */

    inline void update_value(int s, int t, T value) {
        relation[s][t] = value;
    }

    /*
    inline const std::vector<std::vector<T>> &get_relation() {
        return relation;
    }
     */

    T compute_max_value() {
        max_relation_value = 0;
        for (const auto &row: relation) {
            for (T value: row) {
                max_relation_value = std::max(max_relation_value, value);
            }
        }
        return max_relation_value;
    }

    /*
    T get_max_value() const {
        return max_relation_value;
    }
     */

    // std::vector<int> get_dangerous_labels(const LabelledTransitionSystem *lts) const;

    void dump(const std::vector<std::string> &names) const;

    void dump() const;

    void statistics() const;

    /// returns the minimal negative finite entry of the relation table or 0 if no such entry exists
    [[nodiscard]] int get_min_finite_entry() const;

    [[nodiscard]] bool has_dominance() const;

    [[nodiscard]] bool has_positive_dominance() const;

    [[nodiscard]] std::unique_ptr<StrippedNumericSimulationRelation> strip() const;
};

template<>
inline std::unique_ptr<StrippedNumericSimulationRelation> NumericSimulationRelation<int>::strip() const {
    return std::make_unique<StrippedNumericSimulationRelation>(abs->strip(), relation);
}

template<>
inline std::unique_ptr<StrippedNumericSimulationRelation> NumericSimulationRelation<IntEpsilon>::strip() const {
    throw std::logic_error("Stripping NumericSimulationRelation<IntEpsilon> is not supported.");
}

template<>
inline int NumericSimulationRelation<int>::get_min_finite_entry() const {
    int result = 0;
    for (unsigned int i = 0; i < relation.size(); ++i) {
        for (unsigned int j = 0; j < relation.size(); ++j) {
            if (i == j)
                continue;
            const int entry = relation[i][j];
            if (entry != MINUS_INFINITY) {
                result = std::min(entry, result);
            }
        }
    }
    return result;
}

template<>
inline int NumericSimulationRelation<IntEpsilon>::get_min_finite_entry() const {
    throw std::logic_error("NumericSimulationRelation<IntEpsilon> does not support computing the minimal finite value.");
}
}
