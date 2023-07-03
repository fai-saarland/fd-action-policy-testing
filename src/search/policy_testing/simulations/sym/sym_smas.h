#pragma once

#include "../../../operator_cost.h"
#include "smas_shrink_strategy.h"
#include "../merge_and_shrink/abstraction.h"
#include "sym_abstraction.h"
#include "sym_variables.h"
#include <set>
#include <vector>
#include <forward_list>

namespace simulations {
class SMASShrinkState;
class SMASAbsState;

/*
 * SMAS abstraction has a list of SMASShrinkState, which are the
 * result of shrinking several abstract states.
 * At the beginning we have one single shrink state: oneBDD with no variables in the cube.
 * When two abstract states are shrunk together they form a new shrink
 * Abstract states reference shrink states,
 */

class SymSMAS : public SymAbstraction {
    static const int PRUNED_STATE;
    static const int DISTANCE_UNKNOWN;

    const bool is_unit_cost;     //If the abstraction labels are unit cost
    const OperatorCost cost_type;     // OperatorCost considered in the abstraction

    std::vector<std::shared_ptr<SMASShrinkState>> shrinkStates;
    std::vector<std::shared_ptr<SMASAbsState>> absStates;     //BDDs describing each abstract state

    //BDD biimpAbsStateVars;  Abstract TRs are now initialized by shrinking the original TRs

    BDD absVarsCube, absVarsCubep;     //Cube BDD representing abstractedVars (with variables s')

    std::vector<OperatorID> relevant_operators;       // List with all the relevant operators for the abstracted vars
    std::vector<OperatorID> irrelevant_operators;     // List with all irrelevant operators for the abstracted vars
    int num_states;                                         // Current number of states in the abstraction
    std::vector<std::vector<AbstractTransition>> transitions_by_op;      //Transitions in the abstraction

    std::vector<int> init_distances;     //Distances in the abstract state space from I to each state.
    std::vector<int> goal_distances;     //Distances in the abstract state space from G to each state.
    std::vector<bool> goal_states;       //States that are goal
    AbstractStateRef init_state;         //Initial state

    int max_f;
    int max_g;
    int max_h;

    bool are_labels_reduced;

    //We store the value of must_clear_distances to know whether the
    //shrinking was f-preserving or not
    bool f_preserved;

    mutable int peak_memory;     //Memory used by the abstractionx

    //Now, private methods
    void clear_distances();

    void compute_init_distances_unit_cost();

    void compute_goal_distances_unit_cost();

    void compute_init_distances_general_cost();

    void compute_goal_distances_general_cost();

    int unique_unlabeled_transitions() const;

    int count_spurious_states() const;

    // void setAbsBDDsp();

    //Creates atomic abstraction
    SymSMAS(SymVariables *bdd_vars, bool is_unit_cost_,
            OperatorCost cost_type_, int variable);

public:

    bool is_f_preserved() const {
        return f_preserved;
    }

    int total_transitions() const;

    void apply_abstraction(std::vector<AbstractStateRefList> &collapsed_groups);

    virtual int memory_estimate() const;

    //Creates a new sym abstraction of the original state space
    SymSMAS(SymVariables *bdd_vars, bool is_unit_cost_,
            OperatorCost cost_type_);

    //Merges two abstractions
    SymSMAS(SymSMAS *abs1, SymSMAS *abs2, AbsTRsStrategy absTRsStrategy,
            const std::vector<BDD> &notMutexBDDs);

    static void
    build_atomic_abstractions(SymVariables *vars,
                              bool is_unit_cost_, OperatorCost cost_type_,
                              std::vector<SymSMAS *> &result);

    bool is_solvable() const;

    int size() const;

    void statistics(bool include_expensive_statistics) const;

    int get_peak_memory_estimate() const;

    // not implemented and not used
    // bool is_in_varset(int var) const;

    void compute_distances();

    // TODO appears to be broken in original version (is it even needed?=
    // void normalize(bool reduce_labels);

    void release_memory();

    void dump() const;

    // The following methods exist for the benefit of shrink strategies.
    int get_max_f() const;

    int get_max_g() const;

    int get_max_h() const;

    bool is_goal_state(int state) const {
        return goal_states[state];
    }

    int get_init_distance(int state) const {
        return init_distances[state];
    }

    int get_goal_distance(int state) const {
        return goal_distances[state];
    }

    inline unsigned int get_num_ops() const {
        return transitions_by_op.size();
    }

    const std::vector<AbstractTransition> &get_transitions_for_op(int op_no) const {
        return transitions_by_op[op_no];
    }

    int get_cost_for_op(int op_no) const;

    inline unsigned int get_num_relevant_ops() const {
        return relevant_operators.size();
    }

    inline OperatorID get_relevant_operator(int op_no) const {
        return relevant_operators[op_no];
    }

    inline unsigned int get_num_irrelevant_ops() const {
        return irrelevant_operators.size();
    }

    inline OperatorID get_irrelevant_operator(int op_no) const {
        return irrelevant_operators[op_no];
    }

    /*  inline BDD getBiimpAbsStateVars() const {
      return biimpAbsStateVars;
      }
    */

    ~SymSMAS() override = default;

    BDD shrinkExists(const BDD &bdd, int maxNodes) const override;

    BDD shrinkForall(const BDD &bdd, int maxNodes) const override;

    BDD shrinkTBDD(const BDD &tBDD, int maxNodes) const override;

    ADD getExplicitHeuristicADD(bool fw) override;

    void getExplicitHeuristicBDD(bool fw, std::map<int, BDD> &res) override;

    // TODO not clear what to do with this (implementation commented out, hides virtual base function)
    /*virtual void getTransitions(const std::map<int, std::vector<SymTransition> > &individualTRs,
                                std::map<int, std::vector<SymTransition> > &trs,
                                int maxTime, int maxNodes) const; */


    BDD getInitialState() const override;

    BDD getGoal() const override;

    std::string tag() const override;

    void print(std::ostream &os, bool fullInfo) const override;

    friend class SMASShrinkStrategy;     // for apply() -- TODO: refactor!
};
}
