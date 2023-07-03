#pragma once

#include "sym_variables.h"
#include "smas_shrink_state.h"
#include "../utils/debug.h"
// #include "../timer.h"

#include <memory>
#include <utility>

namespace simulations {
class SMASAbsState {
public:
    std::shared_ptr<SMASShrinkState> shrinkState;
    BDD bdd;
    BDD cube;

    //Precomputed BDDs
    BDD totalBDD;

    bool spurious;

    //TODO: there are many options for the detection of spurious
    //abstract states. Right now, we are just checking in every merge if
    //the abstract state is included in any mutex BDD. This does not
    //detect all spurious states. Other option might be to change the
    //BDD of the atomic abstraction pruning the spurious states
    //there. Some control should be made wrt the size of the BDDs
    //though...
    SMASAbsState() : shrinkState(nullptr), spurious(true) {}

    SMASAbsState(std::shared_ptr<SMASShrinkState> ss, SymVariables *vars) :
        shrinkState(std::move(ss)), bdd(vars->oneBDD()), cube(vars->oneBDD()),
        totalBDD(vars->zeroBDD()), spurious(false) {
    }

    SMASAbsState(std::shared_ptr<SMASShrinkState> ss, SymVariables *vars,
                 int variable, int value
                 /*const std::vector<BDD> & notMutexBDDs*/) :
        shrinkState(std::move(ss)), bdd(vars->preBDD(variable, value)),
        cube(vars->getCubePre(variable)),
        totalBDD(vars->zeroBDD()), spurious(false) {
    }

    //Merge two smas abstract states
    SMASAbsState(std::shared_ptr<SMASShrinkState> ss, SymVariables *vars,
                 const std::shared_ptr<SMASAbsState> &o,
                 const std::shared_ptr<SMASAbsState> &o2,
                 const std::vector<BDD> &notMutexBDDs) :
        shrinkState(std::move(ss)), bdd(o->bdd * o2->bdd), cube(o->cube * o2->cube),
        totalBDD(vars->zeroBDD()), spurious(false) {
        getBDD();
        for (const BDD &_bdd: notMutexBDDs) {
            spurious = totalBDD < !_bdd;
            if (spurious) {
                break;
            }
        }
    }

    inline BDD getBDD() {
        if (totalBDD.IsZero()) {
            totalBDD = ((shrinkState->bdd) * bdd);
        }
        return totalBDD;
    }

    [[nodiscard]] inline BDD getCube() const {
        return shrinkState->cube * cube;
    }

    [[nodiscard]] inline bool is_spurious() const {
        return spurious;
    }
};
}
