# pragma once

#include <vector>
#include <string>

#include "../simulations_manager.h"

namespace simulations {
class Labels;
class CompositeAbstraction;
class LabelledTransitionSystem;
class Abstraction;

// First implementation of a simulation relation.
class SimulationRelation {
protected:
    Abstraction *abs;

    //By now we assume that the partition is unitary... we can improve
    //this later with EquivalenceRelation
    std::vector<std::vector<bool>> relation;
    //To compute intermediate simulations.
    //If fixed_relation is set, then we can skip checking it
    std::vector<std::vector<bool>> fixed_relation;

    //Vectors of states dominated/dominating by each state. Lazily
    // computed when needed.
    std::vector<std::vector<int>> dominated_states, dominating_states;

public:
    explicit SimulationRelation(Abstraction *_abs);

    void init_goal_respecting();
    void init_identity();
    void init_incremental(CompositeAbstraction *_abs,
                          const SimulationRelation &simrel_one,
                          const SimulationRelation &simrel_two);

    /*SimulationRelation(CompositeAbstraction *_abs,
                       const SimulationRelation &simrel_one,
                       const SimulationRelation &simrel_two);*/

    void apply_shrinking_to_table(const std::vector<int> &abstraction_mapping);


    [[nodiscard]] bool simulates(const State &t, const State &s) const;

    [[nodiscard]] inline bool simulates(int s, int t) const {
        return !relation.empty() ? relation[s][t] : s == t;
    }

    [[nodiscard]] inline bool strictly_simulates(int s, int t) const {
        return !relation.empty() && relation[s][t] && !relation[t][s];
    }

    [[nodiscard]] bool is_identity() const;

    [[nodiscard]] inline bool similar(int s, int t) const {
        return !relation.empty() ?
               relation[s][t] && relation[t][s] :
               s == t;
    }

    [[nodiscard]] inline bool fixed_simulates(int s, int t) const {
        return !fixed_relation.empty() ? fixed_relation[s][t] : s == t;
    }

    inline void remove(int s, int t) {
        relation[s][t] = false;
    }

    inline const std::vector<std::vector<bool>> &get_relation() {
        return relation;
    }

    [[nodiscard]] int num_equivalences() const;
    [[nodiscard]] int num_simulations(bool ignore_equivalences) const;
    [[nodiscard]] unsigned int num_states() const {
        return relation.size();
    }


    std::vector<int> get_dangerous_labels(const LabelledTransitionSystem *lts) const;
    [[nodiscard]] bool has_positive_dominance() const;

    void reset();
    void dump(const std::vector<std::string> &names) const;
    void dump() const;

    [[nodiscard]] inline const Abstraction *get_abstraction() const {
        return abs;
    }

    [[nodiscard]] const std::vector <int> &get_varset() const;
    [[nodiscard]] bool pruned(const State &state) const;
    [[nodiscard]] int get_cost(const State &state) const;
    [[nodiscard]] int get_index(const State &state) const;
    const std::vector<int> &get_dominated_states(const State &state);
    const std::vector<int> &get_dominating_states(const State &state);

    //Computes the probability of selecting a random pair s, s' such that
    //s is equivalent to s'.
    [[nodiscard]] double get_percentage_equivalences() const;

    void shrink();

    void compute_list_dominated_states();

    void cancel_simulation_computation();
};
}
