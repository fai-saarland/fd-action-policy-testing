#pragma once

// #include "../operator.h"
#include "sym_variables.h"
//#include "../cudd-2.5.0/cudd/cudd.h"
//#include "../cudd-2.5.0/obj/cuddObj.hh"

#include <set>
#include <vector>

namespace simulations {
class SymAbstraction;
class SymManager;
class SymPDB;
class SymSMAS;
class SimulationRelation;
class DominanceRelation;

/*
 * Represents a symbolic transition.
 * It has two differentiated parts: label and abstract state transitions
 * Label refers to variables not considered in the merge-and-shrink
 * Each label has one or more abstract state transitions
 */
class SymTransition {
    SymVariables *sV;     //To call basic BDD creation methods
    int cost;     // transition cost
    BDD tBDD;     // bdd for making the relprod

    std::vector<int> effVars;     //FD Index of eff variables. Must be sorted!!
    BDD existsVars, existsBwVars;         // Cube with variables to existentialize
    std::vector<BDD> swapVarsS, swapVarsSp;     // Swap variables s to sp and vice versa
    std::vector<BDD> swapVarsA, swapVarsAp;     // Swap abstraction variables

    std::set<OperatorID> ops;     //List of operators represented by the TR

    const SymAbstraction *absAfterImage;
public:
    //Constructor for abstraction transitions
    SymTransition(SymManager *mgr,
                  const DominanceRelation &sim_relations);

    //Constructor for transitions irrelevant for the abstraction
    SymTransition(SymVariables *sVars, OperatorID op, int cost_);

    //Copy constructor
    // SymTransition(const SymTransition &) = default;

    [[nodiscard]] BDD image(const BDD &from) const;

    [[nodiscard]] BDD preimage(const BDD &from) const;

    [[nodiscard]] BDD image(const BDD &from, int maxNodes) const;

    [[nodiscard]] BDD preimage(const BDD &from, int maxNodes) const;

    void edeletion(SymManager &mgr);     //Includes mutex into the transition

    void merge(const SymTransition &t2, int maxNodes);

    //shrinks the transition to another abstract state space (useful to preserve edeletion)
    void shrink(const SymAbstraction &abs, int maxNodes);

    bool setMaSAbstraction(SymAbstraction *abs,
                           const BDD &bddSrc, const BDD &bddTarget);

    inline void setAbsAfterImage(const SymAbstraction *abs) {
        absAfterImage = abs;
    }

    [[nodiscard]] inline int getCost() const {
        return cost;
    }

    [[nodiscard]] inline int nodeCount() const {
        return tBDD.nodeCount();
    }

    [[nodiscard]] inline const std::set<OperatorID> &getOps() const {
        return ops;
    }

    [[nodiscard]] static inline bool hasOp(const std::set<OperatorID> &ops_) {
        for (const auto &op: ops_) {
            if (ops_.count(op)) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] inline const BDD &getBDD() const {
        return tBDD;
    }

    friend std::ostream &operator<<(std::ostream &os, const SymTransition &tr);
};
}
