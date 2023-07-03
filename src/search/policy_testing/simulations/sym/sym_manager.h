#pragma once

#include "sym_bucket.h"
#include "sym_abstraction.h"
#include "sym_variables.h"
#include "sym_transition.h"
#include "sym_params.h"
#include "sym_util.h"
#include "../operator_cost.h"

#include <vector>
#include <map>
#include <memory>

namespace simulations {
class SymPruneHeuristic;
/*
 * All the methods may throw exceptions in case the time or nodes are exceeded.
 */


class SymManager {
private:
    SymVariables *vars;
    SymAbstraction *abstraction;
    SymParamsMgr p;
    OperatorCost cost_type;

    //Useful for initialization of mutexes and TRs by relaxation
    SymManager *parentMgr;

    BDD initialState;     // initial state
    BDD goal;     // bdd representing the true (i.e. not simplified) goal-state

    std::map<int, std::vector<SymTransition>> transitions;      //TRs
    int min_transition_cost;     //minimum cost of non-zero cost transitions
    bool hasTR0;     //If there is transitions with cost 0
    //Individual TRs: Useful for shrink and plan construction
    std::map<int, std::vector<SymTransition>> indTRs;

    /* TODO support mutexes
    bool mutexInitialized, mutexByFluentInitialized;

    //BDD representation of valid states (wrt mutex) for fw and bw search
    std::vector<BDD> notMutexBDDsFw, notMutexBDDsBw;

    //Dead ends for fw and bw searches. They are always removed in
    //filter_mutex (it does not matter which mutex_type we are using).
     */
    std::vector<BDD> notDeadEndFw, notDeadEndBw;

    /* TODO support mutexes
    //notMutex relative for each fluent
    std::vector<std::vector<BDD>> notMutexBDDsByFluentFw, notMutexBDDsByFluentBw;
    std::vector<std::vector<BDD>> exactlyOneBDDsByFluent;
     */

    SymPruneHeuristic *prune_heuristic;

    void init_states();

    void zero_preimage(const BDD &bdd, std::vector<BDD> &res, int maxNodes) const;

    void cost_preimage(const BDD &bdd, std::map<int, std::vector<BDD>> &res, int maxNodes) const;

    void zero_image(const BDD &bdd, std::vector<BDD> &res, int maxNodes) const;

    void cost_image(const BDD &bdd, std::map<int, std::vector<BDD>> &res, int maxNodes) const;

public:
    SymManager(SymManager *mgr, SymAbstraction *abs, const SymParamsMgr &params);

    SymManager(SymVariables *v, SymAbstraction *abs, const SymParamsMgr &params, OperatorCost cost_type_);

    void init() {
        // init_mutex(g_mutex_groups); TODO include mutexes
        init_transitions();
        init_simulation();
    }

    // TODO mutexes not yet supported!
    // Be careful of calling init_mutex and init_transitions before actually calling filter_mutex or image
    /* void init_mutex(const std::vector<MutexGroup> &mutex_groups);
    void init_mutex(const std::vector<MutexGroup> &mutex_groups,
                    bool genMutexBDDs, bool genMutexBDDsByFluent);
    void init_mutex(const std::vector<MutexGroup> &mutex_groups,
                    bool genMutexBDD, bool genMutexBDDByFluent, bool fw); */

    void init_simulation();

    //void mutexRegression(int maxTime, int maxNodes);
    void init_transitions();

    const std::map<int, std::vector<SymTransition>> &getIndividualTRs();


    /* TODO include mutexes
    void filterMutex(Bucket &bucket, bool fw, bool initialization) {
        filterMutexBucket(bucket, fw, initialization,
                          p.max_pop_time, p.max_pop_nodes);
    }*/

    void mergeBucket(Bucket &bucket) {
        mergeBucket(bucket, p.max_pop_time, p.max_pop_nodes);
    }

    void mergeBucketAnd(Bucket &bucket) {
        mergeBucketAnd(bucket, p.max_pop_time, p.max_pop_nodes);
    }

    [[nodiscard]] double stateCount(const Bucket &bucket) const {
        double sum = 0;
        for (const BDD &bdd: bucket) {
            sum += vars->numStates(bdd);
        }
        return sum;
    }


    void shrinkBucket(Bucket &bucket, int maxNodes) const {
        for (auto &i : bucket) {
            i = shrinkExists(i, maxNodes);
        }
    }


    bool mergeBucket(Bucket &bucket, int maxTime, int maxNodes) {
        auto mergeBDDs = [](const BDD &bdd, const BDD &bdd2, int maxNodes) {
                return bdd.Or(bdd2, maxNodes);
            };
        merge(vars, bucket, mergeBDDs, maxTime, maxNodes);
        removeZero(bucket);     //Be sure that we do not contain only the zero BDD

        return bucket.size() <= 1;
    }

    bool mergeBucketAnd(Bucket &bucket, int maxTime, int maxNodes) {
        auto mergeBDDs = [](const BDD &bdd, const BDD &bdd2, int maxNodes) {
                return bdd.And(bdd2, maxNodes);
            };
        merge(vars, bucket, mergeBDDs, maxTime, maxNodes);
        removeZero(bucket);     //Be sure that we do not contain only the zero BDD

        return bucket.size() <= 1;
    }

    void addDeadEndStates(bool fw, BDD bdd);

    void addDeadEndStates(const std::vector<BDD> &fw_dead_ends,
                          const std::vector<BDD> &bw_dead_ends);


    [[nodiscard]] inline BDD shrinkExists(const BDD &bdd, int maxNodes) const {
        return abstraction->shrinkExists(bdd, maxNodes);
    }

    [[nodiscard]] inline BDD shrinkForall(const BDD &bdd, int maxNodes) const {
        return abstraction->shrinkForall(bdd, maxNodes);
    }

    inline BDD shrinkForall(const BDD &bdd) {
        setTimeLimit(p.max_pop_time);
        try {
            BDD res = abstraction->shrinkForall(bdd, p.max_pop_nodes);
            unsetTimeLimit();
            return res;
        } catch (BDDError &e) {
            unsetTimeLimit();
        }
        return zeroBDD();
    }


    [[nodiscard]] inline long totalNodes() const {
        return vars->totalNodes();
    }

    [[nodiscard]] inline unsigned long totalMemory() const {
        return vars->totalMemory();
    }

    inline const BDD &getGoal() {
        if (goal.IsZero()) {
            init_states();
        }
        return goal;
    }

    inline const BDD &getInitialState() {
        if (initialState.IsZero()) {
            init_states();
        }
        return initialState;
    }

    //Update binState
    [[nodiscard]] inline int *getBinaryDescription(const State &state) const {
        return vars->getBinaryDescription(state);
    }

    [[nodiscard]] inline BDD getBDD(int variable, int value) const {
        return vars->preBDD(variable, value);
    }

    [[nodiscard]] inline Cudd *mgr() const {
        return vars->mgr();
    }

    [[nodiscard]] inline BDD zeroBDD() const {
        return vars->zeroBDD();
    }

    [[nodiscard]] inline BDD oneBDD() const {
        return vars->oneBDD();
    }

    /* TODO include mutexes
    inline const std::vector<BDD> &getNotMutexBDDs(bool fw) {
        init_mutex(g_mutex_groups);
        return (fw ? notMutexBDDsFw : notMutexBDDsBw);
    } */

    [[nodiscard]] inline const std::vector<int> &vars_index_pre(int variable) const {
        return vars->vars_index_pre(variable);
    }

    [[nodiscard]] inline const std::vector<int> &vars_index_eff(int variable) const {
        return vars->vars_index_eff(variable);
    }

    [[nodiscard]] inline const std::vector<int> &vars_index_abs(int variable) const {
        return vars->vars_index_abs(variable);
    }

    [[nodiscard]] inline SymVariables *getVars() const {
        return vars;
    }

    [[nodiscard]] inline SymAbstraction *getAbstraction() const {
        return abstraction;
    }

    [[nodiscard]] inline const SymParamsMgr &getParams() const {
        return p;
    }

    void dumpMutexBDDs(bool fw) const;

    //Methods that require of TRs initialized
    [[nodiscard]] inline int getMinTransitionCost() const {
        //init_transitions(); WARNING: not initializing transitions may return a lower min transition cost
        return min_transition_cost;
    }

    [[nodiscard]] inline bool hasTransitions0() const {
        //init_transitions(); WARNING: not initializing transitions may cause the mgr to say that has 0-cost transitions even if it does not (the 0 cost transitions are related to non-abstracted variables
        return hasTR0;
    }

    [[nodiscard]] inline const std::map<int, std::vector<SymTransition>> &getTransitions() const {
        return transitions;
    }

    [[nodiscard]] inline const std::map<int, std::vector<SymTransition>> &getIndividualTransitions() const {
        return indTRs;
    }

    inline void zero_image(bool fw,
                           const BDD &bdd, std::vector<BDD> &res,
                           int maxNodes) {
        init_transitions();
        if (fw)
            zero_image(bdd, res, maxNodes);
        else
            zero_preimage(bdd, res, maxNodes);
    }

    inline void cost_image(bool fw,
                           const BDD &bdd, std::map<int, std::vector<BDD>> &res,
                           int maxNodes) {
        init_transitions();
        if (fw) {
            cost_image(bdd, res, maxNodes);
        } else {
            cost_preimage(bdd, res, maxNodes);
        }
    }

    /* TODO include mutexes
    //Methods that require of mutex initialized
    inline const BDD &getNotMutexBDDFw(int var, int val) {
        init_mutex(g_mutex_groups, false, true);
        return notMutexBDDsByFluentFw[var][val];
    }
    //Methods that require of mutex initialized
    inline const BDD &getNotMutexBDDBw(int var, int val) {
        init_mutex(g_mutex_groups, false, true);
        return notMutexBDDsByFluentBw[var][val];
    }
    //Methods that require of mutex initialized
    inline const BDD &getExactlyOneBDD(int var, int val) {
        init_mutex(g_mutex_groups, false, true);
        return exactlyOneBDDsByFluent[var][val];
    }
    BDD filter_mutex(const BDD &bdd,
                     bool fw, int maxNodes,
                     bool initialization);
    int filterMutexBucket(std::vector<BDD> &bucket, bool fw,
                          bool initialization, int maxTime, int maxNodes);
    */


    inline void setTimeLimit(int maxTime) {
        vars->setTimeLimit(maxTime);
    }

    inline void unsetTimeLimit() {
        vars->unsetTimeLimit();
    }

    /*
    void set_simulation(SymPruneHeuristic *prune) {
        prune_heuristic = prune;
        if (mutexInitialized) {
            init_simulation();
        }
    } */

    BDD simulatedBy(const BDD &bdd, bool fw);
};
}
