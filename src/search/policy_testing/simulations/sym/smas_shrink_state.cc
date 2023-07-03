#include "smas_shrink_state.h"

#include "sym_util.h"
#include "smas_abs_state.h"

namespace simulations {
SMASShrinkState::SMASShrinkState(SymVariables *vars,
                                 std::vector<std::shared_ptr<SMASAbsState>> &absStates,
                                 const std::list<AbstractStateRef> &group) :
    bdd(vars->zeroBDD()),
    cube(vars->oneBDD()), marked(false) {
    std::vector<BDD> bdds;
    bdds.reserve(group.size());
    for (auto &pos: group) {
        bdds.push_back(absStates[pos]->getBDD());
        //bdd += absStates[pos].getBDD();
        cube *= absStates[pos]->getCube();
    }

    merge(vars, bdds, mergeOrBDD, std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
    bdd = bdds[0];
}

std::ostream &operator<<(std::ostream &os, const SMASShrinkState &other) {
    return os << "SS(" << other.bdd.nodeCount() << ", " << other.cube.nodeCount() << ")";
}
}
