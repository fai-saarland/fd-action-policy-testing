#pragma once

#include "sym_variables.h"
#include <set>
#include <map>
#include <string>
#include <iostream>

#include "sym_enums.h"

namespace simulations {
class SymTransition;

class SymAbstraction {
protected:
    SymVariables *vars;     //BDD variables to perform operations

    //If the variable is fully/partially/not considered in the abstraction
    std::set<int> fullVars, absVars, nonRelVars;

    AbsTRsStrategy absTRsStrategy;
public:
    SymAbstraction(SymVariables *_vars, AbsTRsStrategy _absTRs) :
        vars(_vars), absTRsStrategy(_absTRs) {}

    virtual ~SymAbstraction() = default;

    [[nodiscard]] virtual BDD shrinkExists(const BDD &bdd, int maxNodes) const = 0;

    [[nodiscard]] virtual BDD shrinkForall(const BDD &bdd, int maxNodes) const = 0;

    [[nodiscard]] virtual BDD shrinkTBDD(const BDD &bdd, int maxNodes) const = 0;


    [[nodiscard]] virtual BDD getInitialState() const = 0;

    [[nodiscard]] virtual BDD getGoal() const = 0;

    virtual ADD getExplicitHeuristicADD(bool fw) = 0;

    virtual void getExplicitHeuristicBDD(bool fw, std::map<int, BDD> &res) = 0;

    virtual void getTransitions(const std::map<int, std::vector<SymTransition>> & /*individualTRs*/,
                                std::map<int, std::vector<SymTransition>> & /*res*/) const {
        std::cerr << "REBUILD TRs not supported by " << *this << std::endl;
        exit(-1);
    }

    void shrinkTransitions(const std::map<int, std::vector<SymTransition>> &trs,
                           const std::map<int, std::vector<SymTransition>> &indTRs,
                           std::map<int, std::vector<SymTransition>> &res,
                           int maxTime, int maxNodes) const;

    [[nodiscard]] inline SymVariables *getVars() const {
        return vars;
    }

    [[nodiscard]] inline const std::set<int> &getFullVars() const {
        return fullVars;
    }

    [[nodiscard]] inline const std::set<int> &getAbsVars() const {
        return absVars;
    }

    [[nodiscard]] inline const std::set<int> &getNonRelVars() const {
        return nonRelVars;
    }

    [[nodiscard]] inline bool isRelevantVar(int var) const {
        return fullVars.count(var) > 0 || absVars.count(var);
    }

    [[nodiscard]] inline bool isAbstracted() const {
        //return true;
        return !(absVars.empty() && nonRelVars.empty());
    }

    [[nodiscard]] unsigned int numVariablesToAbstract() const {
        return fullVars.size();
    }

    [[nodiscard]] unsigned int numVariablesAbstracted() const {
        return absVars.size() + nonRelVars.size();
    }

    [[nodiscard]] BDD getRelVarsCubePre() const {
        return vars->getCubePre(fullVars) + vars->getCubePre(absVars);
    }

    [[nodiscard]] BDD getRelVarsCubeEff() const {
        return vars->getCubeEff(fullVars) + vars->getCubeEff(absVars);
    }

    friend std::ostream &operator<<(std::ostream &os, const SymAbstraction &abs);

    virtual void print(std::ostream &os, bool /*fullInfo*/) const {
        os << "Undefined print: " << tag() << std::endl;
    }

    [[nodiscard]] virtual std::string tag() const = 0;
};
}
