#pragma once

// #include "../prune_heuristic.h"
#ifdef INCLUDE_SYM
#include "../sym/sym_variables.h"
#include "../sym/sym_params.h"
#endif

#include "tau_labels.h"
#include "int_epsilon.h"
#include "numeric_dominance_relation.h"

namespace simulations {
class LDSimulation;
class AbstractionBuilder;
#ifdef INCLUDE_SYM
class SymVariables;
class SymManager;
#endif
class Abstraction;

template<typename T>
//class NumericDominancePruning : public PruneHeuristic {
class NumericDominancePruning {
    OperatorCost cost_type;
protected:
    //Parameters to control the pruning
#ifdef INCLUDE_SYM
    const SymParamsMgr mgrParams;     //Parameters for SymManager configuration.
#endif

    bool initialized;
    std::shared_ptr<TauLabelManager<T>> tau_labels;
    const bool remove_spurious_dominated_states;
    const bool insert_dominated;
    const bool use_quantified_dominance;
    const bool trade_off_dominance;
    const bool only_positive_dominance;
    const bool use_ADDs;

    const bool prune_dominated_by_parent;
    const bool prune_dominated_by_initial_state;
    const bool prune_successors;
    const bool prune_dominated_by_closed;
    const bool prune_dominated_by_open;

    const int truncate_value;
    const int max_simulation_time;
    const int min_simulation_time;
    const int max_total_time;

    const int max_lts_size_to_compute_simulation;
    const int num_labels_to_use_dominates_in;

    /*
   * Three parameters help to decide whether to apply dominance
   * pruning or not. Dominance pruning is used until
   * min_insertions_desactivation are performed. At that moment, if
   * the ratio pruned/checked is lower than min_desactivation_ratio
   * the pruning is desactivated. If not, the pruning remains
   * activated until the planner finishes.
   */
    const int min_insertions_desactivation;
    const double min_desactivation_ratio;

    const bool dump;
    const bool exit_after_preprocessing;

#ifdef INCLUDE_SYM
    std::unique_ptr<SymVariables> vars;     //The symbolic variables are declared here
    std::unique_ptr<SymManager> mgr;        //The symbolic manager to handle mutex BDDs
#endif

    std::unique_ptr<AbstractionBuilder> abstractionBuilder;
    std::unique_ptr<LDSimulation> ldSimulation;
    std::unique_ptr<NumericDominanceRelation<T>> numeric_dominance_relation;
    std::vector<std::unique_ptr<Abstraction>> abstractions;

    bool all_desactivated;
    bool activation_checked;

    int states_inserted;     //Count the number of states inserted
    int states_checked;     //Count the number of states inserted
    int states_pruned;     //Count the number of states pruned
    int deadends_pruned;     //Count the number of dead ends detected

    void dump_options() const;

    [[nodiscard]] bool apply_pruning() const;

#ifdef INCLUDE_SYM
    /* Methods to help concrete classes */
    BDD getBDDToInsert(const State &state);

    std::map<int, BDD> getBDDMapToInsert(const State &state);
#endif

    //Methods to keep dominated states in explicit search
    //Check: returns true if a better or equal state is known
    virtual bool check(const State &state, int g) = 0;

    virtual void insert(const State &state, int g) = 0;

    inline bool is_activated() {
        if (!activation_checked && states_inserted > min_insertions_desactivation) {
            activation_checked = true;
            all_desactivated = states_pruned == 0 ||
                states_pruned < states_checked * min_desactivation_ratio;
            std::cout << "Simulation pruning " << (all_desactivated ? "desactivated: " : "activated: ")
                      << states_pruned << " pruned " << states_checked << " checked " <<
                states_inserted << " inserted " << deadends_pruned << " deadends " << std::endl;
        }
        return !all_desactivated;
    }

public:
    //virtual void initialize(bool force_initialization = false) override;
    void initialize(bool force_initialization = false);

    //Methods for pruning explicit search
    // virtual void prune_applicable_operators(const State & state, int g, std::vector<const Operator *> & operators, SearchProgress & search_progress) override;
    // virtual bool prune_generation(const State &state, int g, const State &parent, int action_cost) override;
    // virtual bool prune_expansion (const State &state, int g) override;

    //virtual bool is_dead_end(const State &state) override;
    bool is_dead_end(const State &state);

    //virtual int compute_heuristic(const State &state) override;
    int compute_heuristic(const State &state);

    explicit NumericDominancePruning(const options::Options &opts);

    virtual ~NumericDominancePruning() = default;

    //virtual void print_statistics() override;
    void print_statistics();

    /*virtual bool proves_task_unsolvable() const override {
        return true;
    }*/

    //virtual bool strictly_dominates_initial_state(const State &) const override;

    //virtual void restart_search(const State & new_initial_state) override;
};


#ifdef INCLUDE_SYM
template<typename T>
class NumericDominancePruningBDDMap : public NumericDominancePruning<T> {
    std::map<int, BDD> closed;
public:
    explicit NumericDominancePruningBDDMap(const options::Options &opts) :
        NumericDominancePruning<T>(opts) {}

    virtual ~NumericDominancePruningBDDMap() = default;

    //Methods to keep dominated states in explicit search
    bool check(const State &state, int g) override;

    void insert(const State &state, int g) override;
};


template<typename T>
class NumericDominancePruningBDD : public NumericDominancePruning<T> {
    BDD closed, closed_inserted;
    bool initialized;

public:
    explicit NumericDominancePruningBDD(const options::Options &opts) :
        NumericDominancePruning<T>(opts), initialized(false) {}

    virtual ~NumericDominancePruningBDD() = default;

    //Methods to keep dominated states in explicit search
    bool check(const State &state, int g) override;

    void insert(const State &state, int g) override;
};
#endif
}
