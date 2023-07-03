#pragma once

#include "sym_abstraction.h"
#include "sym_variables.h"
#include <set>

namespace simulations {
class SymPDB : public SymAbstraction {
    BDD nonRelVarsCube; //Cube BDD representing relevantVars
    BDD nonRelVarsCubeWithPrimes; //Cube BDD representing relevantVars
    std::string abstractionName;

public:
    explicit SymPDB(SymVariables *bdd_vars); //Creates a BDD with all variables relevant
    SymPDB(SymVariables *bdd_vars, AbsTRsStrategy absTRsStrategy, const std::set<int> &relVars);
    ~SymPDB() override = default;
    BDD shrinkExists(const BDD &bdd, int maxNodes) const override;
    BDD shrinkForall(const BDD &bdd, int maxNodes) const override;
    BDD shrinkTBDD(const BDD &tBDD, int maxNodes) const override;

    ADD getExplicitHeuristicADD(bool fw) override;
    void getExplicitHeuristicBDD(bool fw, std::map<int, BDD> &res) override;

    //virtual void getTransitions(std::map<int, std::vector <SymTransition> > & res) const;
    BDD getInitialState() const override;
    BDD getGoal() const override;
    std::string tag() const override;

    void print(std::ostream &os, bool fullInfo) const override;
};
}
