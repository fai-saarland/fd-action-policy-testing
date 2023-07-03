#pragma once

#include "../cudd-2.5.0-64/include/cuddObj.hh"

#include "../../../task_proxy.h"
#include <cmath>
#include <memory>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <set>
#include <string>
#include <map>

namespace simulations {
/*
 * BDD-Variables for a symbolic exploration.
 * This information is global for every class using symbolic search.
 * The only decision fixed here is the variable ordering, which is assumed to be always fixed.
 */
struct BDDError : std::exception {
};

extern void exceptionError(std::string message);

class SymParamsMgr;

class SymVariables {
    std::unique_ptr<Cudd> _manager;     //_manager associated with this symbolic search

    int numBDDVars;     //Number of binary variables (just one set, the total number is numBDDVars*3
    std::vector<BDD> variables;     // BDD variables

    //The variable order must be complete.
    std::vector<int> var_order;     //Variable(FD) order in the BDD
    std::vector<std::vector<int>> bdd_index_pre, bdd_index_eff, bdd_index_abs;      //vars(BDD) for each var(FD)

    std::vector<std::vector<BDD>> preconditionBDDs;      // BDDs associated with the precondition of a predicate
    std::vector<std::vector<BDD>> effectBDDs;            // BDDs associated with the effect of a predicate
    std::vector<BDD> biimpBDDs;        //BDDs associated with the biimplication of one variable(FD)
    std::vector<BDD> validValues;     // BDD that represents the valid values of all the variables
    BDD validBDD;        // BDD that represents the valid values of all the variables


    //Vector to store the binary description of an state
    //Avoid allocating memory during heuristic evaluation
    std::vector<int> binState;

public:
    void init(const std::vector<int> &v_order, const SymParamsMgr &params);

    [[nodiscard]] BDD readBDD(const std::string &filename) const;

    void readBucket(std::ifstream &filenames,
                    std::vector<BDD> &bucket) const;

    static void writeBucket(const std::string &filename,
                            std::ofstream &filenames,
                            const std::vector<BDD> &bucket);

    static void writeMap(const std::string &fname,
                         std::ofstream &filenames,
                         const std::map<int, BDD> &m);

    void readMap(std::ifstream &filenames,
                 std::map<int, BDD> &m) const;


    static void writeMapBucket(const std::string &fname,
                               std::ofstream &filenames,
                               const std::map<int, std::vector<BDD>> &mb);

    void readMapBucket(std::ifstream &filenames,
                       std::map<int, std::vector<BDD>> &mb) const;

    //State getStateFrom(const BDD & bdd) const;
    [[nodiscard]] BDD getStateBDD(const State &state) const;
    [[nodiscard]] BDD getStateBDD(const std::vector<int> &state) const;

    [[nodiscard]] BDD getPartialStateBDD(const std::vector<std::pair<int, int>> &state) const;

    [[nodiscard]] double numStates(const BDD &bdd) const;     //Returns the number of states in a BDD
    [[nodiscard]] double numStates() const;

    [[nodiscard]] double percentageNumStates(const BDD &bdd) const {
        return numStates(bdd) / numStates();
    }

    [[nodiscard]] bool isIn(const State &state, const BDD &bdd) const;


    [[nodiscard]] inline const std::vector<int> &vars_index_pre(int variable) const {
        return bdd_index_pre[variable];
    }

    [[nodiscard]] inline const std::vector<int> &vars_index_eff(int variable) const {
        return bdd_index_eff[variable];
    }

    [[nodiscard]] inline const std::vector<int> &vars_index_abs(int variable) const {
        return bdd_index_abs[variable];
    }

    [[nodiscard]] inline const BDD &preBDD(int variable, int value) const {
        return preconditionBDDs[variable][value];
    }

    [[nodiscard]] inline const BDD &effBDD(int variable, int value) const {
        return effectBDDs[variable][value];
    }


    [[nodiscard]] inline BDD getCubePre(int var) const {
        return getCube(var, bdd_index_pre);
    }

    [[nodiscard]] inline BDD getCubePre(const std::set<int> &vars) const {
        return getCube(vars, bdd_index_pre);
    }

    [[nodiscard]] inline BDD getCubeEff(int var) const {
        return getCube(var, bdd_index_eff);
    }

    [[nodiscard]] inline BDD getCubeEff(const std::set<int> &vars) const {
        return getCube(vars, bdd_index_eff);
    }


    [[nodiscard]] inline BDD getCubeAbs(int var) const {
        return getCube(var, bdd_index_abs);
    }

    [[nodiscard]] inline BDD getCubeAbs(const std::set<int> &vars) const {
        return getCube(vars, bdd_index_abs);
    }


    [[nodiscard]] inline const BDD &biimp(int variable) const {
        return biimpBDDs[variable];
    }

    [[nodiscard]] inline long totalNodes() const {
        return _manager->ReadNodeCount();
    }

    [[nodiscard]] inline std::vector<BDD> getBDDVarsPre() const {
        return getBDDVars(var_order, bdd_index_pre);
    }

    [[nodiscard]] inline std::vector<BDD> getBDDVarsEff() const {
        return getBDDVars(var_order, bdd_index_eff);
    }

    [[nodiscard]] inline std::vector<BDD> getBDDVarsAbs() const {
        return getBDDVars(var_order, bdd_index_abs);
    }

    [[nodiscard]] inline std::vector<BDD> getBDDVarsPre(const std::vector<int> &vars) const {
        return getBDDVars(vars, bdd_index_pre);
    }

    [[nodiscard]] inline std::vector<BDD> getBDDVarsEff(const std::vector<int> &vars) const {
        return getBDDVars(vars, bdd_index_eff);
    }

    [[nodiscard]] inline std::vector<BDD> getBDDVarsAbs(const std::vector<int> &vars) const {
        return getBDDVars(vars, bdd_index_abs);
    }

    [[nodiscard]] inline unsigned long totalMemory() const {
        return _manager->ReadMemoryInUse();
    }

    [[nodiscard]] inline BDD zeroBDD() const {
        return _manager->bddZero();
    }

    [[nodiscard]] inline BDD oneBDD() const {
        return _manager->bddOne();
    }

    [[nodiscard]] inline BDD validStates() const {
        return validBDD;
    }

    [[nodiscard]] inline Cudd *mgr() const {
        return _manager.get();
    }

    [[nodiscard]] inline BDD bddVar(int index) const {
        return variables[index];
    }

    [[nodiscard]] inline int usedNodes() const {
        return _manager->ReadSize();
    }

    inline void setTimeLimit(int maxTime) {
        _manager->SetTimeLimit(maxTime);
        _manager->ResetStartTime();
    }

    inline void unsetTimeLimit() {
        _manager->UnsetTimeLimit();
    }

    void print();

    inline int *getBinaryDescription(const State &state) {
        int pos = 0;
        for (int v : var_order) {
            for (unsigned int j = 0; j < bdd_index_pre[v].size(); j++) {
                binState[pos++] = ((state[v].get_value() >> j) % 2);
                binState[pos++] = 0;     //Skip interleaving variable
            }
        }
        return &(binState[0]);
    }

    inline ADD getADD(int value) {
        return _manager->constant(value);
    }

    inline ADD getADD(const std::map<int, BDD> &heur) {
        ADD h = getADD(-1);
        for (const auto &entry: heur) {
            int distance = 1 + entry.first;
            h += entry.second.Add() * getADD(distance);
        }
        return h;
    }


private:
    // Auxiliary function helping to create precondition and effect BDDs
    // Generates value for bddVars.
    [[nodiscard]] BDD generateBDDVar(const std::vector<int> &_bddVars, int value) const;

    [[nodiscard]] BDD getCube(int var, const std::vector<std::vector<int>> &v_index) const;

    [[nodiscard]] BDD getCube(const std::set<int> &vars, const std::vector<std::vector<int>> &v_index) const;

    [[nodiscard]] BDD createBiimplicationBDD(const std::vector<int> &vars, const std::vector<int> &vars2) const;

    [[nodiscard]] std::vector<BDD> getBDDVars(const std::vector<int> &vars,
                                              const std::vector<std::vector<int>> &v_index) const;


    [[nodiscard]] inline BDD createPreconditionBDD(int variable, int value) const {
        return generateBDDVar(bdd_index_pre[variable], value);
    }

    [[nodiscard]] inline BDD createEffectBDD(int variable, int value) const {
        return generateBDDVar(bdd_index_eff[variable], value);
    }

    [[nodiscard]] inline int getNumBDDVars() const {
        return numBDDVars;
    }
};
}
