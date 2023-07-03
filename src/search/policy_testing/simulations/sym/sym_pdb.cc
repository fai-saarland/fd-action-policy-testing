#include "sym_pdb.h"

#include "sym_transition.h"

#include "../simulations_manager.h"

namespace simulations {
SymPDB::SymPDB(SymVariables *bdd_vars) :
    SymAbstraction(bdd_vars, AbsTRsStrategy(0)) {
    for (int i = 0; i < global_simulation_task()->get_num_variables(); i++) {
        fullVars.insert(i);
    }

    nonRelVarsCube = bdd_vars->oneBDD();
    nonRelVarsCubeWithPrimes = bdd_vars->oneBDD();
    if (!nonRelVarsCube.IsCube()) {
        std::cout << "Error in sym_pdb: nonRelVars should be a cube";
        nonRelVarsCube.print(0, 1);
        std::cout << std::endl;
        exit(-1);
    }
}

SymPDB::SymPDB(SymVariables *_vars,
               AbsTRsStrategy absTRsStrategy,
               const std::set<int> &relevantVars) :
    SymAbstraction(_vars, absTRsStrategy) {
    fullVars = relevantVars;
    for (int i = 0; i < global_simulation_task()->get_num_variables(); i++) {
        if (!fullVars.count(i)) {
            nonRelVars.insert(i);
        }
    }

    nonRelVarsCube = _vars->getCubePre(nonRelVars);    // * vars->getCubep(nonRelVars);
    nonRelVarsCubeWithPrimes = nonRelVarsCube * _vars->getCubeEff(nonRelVars);
    if (!nonRelVarsCube.IsCube()) {
        std::cout << "Error in sym_pdb: nonRelVars should be a cube";
        nonRelVarsCube.print(0, 1);
        std::cout << std::endl;
        exit(-1);
    }
}

BDD SymPDB::shrinkExists(const BDD &bdd, int maxNodes) const {
    return bdd.ExistAbstract(nonRelVarsCube, maxNodes);
}

BDD SymPDB::shrinkTBDD(const BDD &bdd, int maxNodes) const {
    return bdd.ExistAbstract(nonRelVarsCubeWithPrimes, maxNodes);
}

BDD SymPDB::shrinkForall(const BDD &bdd, int maxNodes) const {
    return bdd.UnivAbstract(nonRelVarsCube, maxNodes);
}

BDD SymPDB::getInitialState() const {
    std::vector<std::pair<int, int>> abstract_ini;
    for (int var: fullVars) {
        abstract_ini.emplace_back(var, global_simulation_task()->get_initial_state_values()[var]);
    }
    return vars->getPartialStateBDD(abstract_ini);
}

BDD SymPDB::getGoal() const {
    std::vector<std::pair<int, int>> abstract_goal;
    const int num_goals = global_simulation_task()->get_num_goals();
    for (int i = 0; i < num_goals; ++i) {
        auto goal = global_simulation_task()->get_goal_fact(i);
        if (isRelevantVar(goal.var)) {
            abstract_goal.emplace_back(goal.var, goal.value);
        }
    }
    return vars->getPartialStateBDD(abstract_goal);
}

std::string SymPDB::tag() const {
    return "PDB";
}

void SymPDB::print(std::ostream &os, bool fullInfo) const {
    os << "PDB (" << fullVars.size() << "/" << (nonRelVars.size() + fullVars.size()) << "): ";
    for (int v: fullVars) {
        os << v << " ";
    }
    if (fullInfo && !nonRelVars.empty()) {
        os << " [";
        for (int v: fullVars)
            os << v << " ";
        os << "]";
        os << std::endl << "Abstracted propositions: ";
        for (int v: nonRelVars) {
            os << v << ": ";
            for (int val = 0; val < global_simulation_task()->get_variable_domain_size(v); ++val) {
                // Changed behaviour (print to os instead of std::cout)
                os << global_simulation_task()->get_fact_name(FactPair(v, val)) << ", ";
            }
            os << std::endl;
        }
        os << std::endl << "Considered propositions: ";
        for (int v: fullVars) {
            os << v << ": ";
            for (int val = 0; val < global_simulation_task()->get_variable_domain_size(v); ++val) {
                // Changed behaviour (print to os instead of std::cout)
                os << global_simulation_task()->get_fact_name(FactPair(v, val)) << ", ";
            }
            os << std::endl;
        }
        os << std::endl;
    }
}

ADD SymPDB::getExplicitHeuristicADD(bool /*fw*/) {
    return vars->getADD(0);
}

void SymPDB::getExplicitHeuristicBDD(bool /*fw*/, std::map<int, BDD> &res) {
    res[0] = vars->oneBDD();
}
}
