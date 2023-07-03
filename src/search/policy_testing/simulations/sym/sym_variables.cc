#include "sym_variables.h"

#include "sym_params.h" //Needed to get the parameters of CUDD init
#include <memory>
#include <iostream>
#include <string>
#include "sym_util.h"

#include "../simulations_manager.h"

namespace simulations {
//Constructor that makes use of global variables to initialize the symbolic_search structures
void SymVariables::init(const std::vector<int> &v_order, const SymParamsMgr &params) {
    std::cout << "Initializing Symbolic Variables" << std::endl;
    var_order = std::vector<int>(v_order);
    const unsigned int num_fd_vars = var_order.size();

    //Initialize binary representation of variables.
    numBDDVars = 0;
    bdd_index_pre = std::vector<std::vector<int>>(v_order.size());
    bdd_index_eff = std::vector<std::vector<int>>(v_order.size());
    bdd_index_abs = std::vector<std::vector<int>>(v_order.size());

    int _numBDDVars = 0;    // numBDDVars;
    for (int var: var_order) {
        int var_len = ceil(log2(global_simulation_task()->get_variable_domain_size(var)));
        numBDDVars += var_len;
        for (int j = 0; j < var_len; j++) {
            bdd_index_pre[var].push_back(_numBDDVars);
            bdd_index_eff[var].push_back(_numBDDVars + 1);
            _numBDDVars += 2;
        }
    }
    std::cout << "Num variables: " << var_order.size() << " => " << numBDDVars << std::endl;

    //Initialize manager
    std::cout << "Initialize Symbolic Manager(" << _numBDDVars << ", "
              << params.cudd_init_nodes / _numBDDVars << ", "
              << params.cudd_init_cache_size << ", "
              << params.cudd_init_available_memory << ")" << std::endl;
    _manager = std::make_unique<Cudd>(_numBDDVars, 0,
                                      params.cudd_init_nodes / _numBDDVars,
                                      params.cudd_init_cache_size,
                                      params.cudd_init_available_memory);

    _manager->setHandler(exceptionError);
    _manager->setTimeoutHandler(exceptionError);
    _manager->setNodesExceededHandler(exceptionError);

    std::cout << "Generating binary variables" << std::endl;
    //Generate binary_variables
    for (int i = 0; i < _numBDDVars; i++) {
        variables.push_back(_manager->bddVar(i));
    }

    std::cout << "Generating predicate BDDs" << std::endl;
    preconditionBDDs.resize(num_fd_vars);
    effectBDDs.resize(num_fd_vars);
    biimpBDDs.resize(num_fd_vars);
    validValues.resize(num_fd_vars);
    validBDD = oneBDD();
    //Generate predicate (precondition (s) and effect (s')) BDDs
    for (int var: var_order) {
        for (int j = 0; j < global_simulation_task()->get_variable_domain_size(var); j++) {
            preconditionBDDs[var].push_back(createPreconditionBDD(var, j));
            effectBDDs[var].push_back(createEffectBDD(var, j));
        }
        validValues[var] = zeroBDD();
        for (int j = 0; j < global_simulation_task()->get_variable_domain_size(var); j++) {
            validValues[var] += preconditionBDDs[var][j];
        }
        validBDD *= validValues[var];
        biimpBDDs[var] = createBiimplicationBDD(bdd_index_pre[var], bdd_index_eff[var]);
    }

    binState.resize(_numBDDVars, 0);
    std::cout << "Symbolic Variables... Done." << std::endl;
}


BDD SymVariables::getStateBDD(const State &state) const {
    BDD res = _manager->bddOne();
    for (int i = var_order.size() - 1; i >= 0; i--) {
        res = res * preconditionBDDs[var_order[i]][state[var_order[i]].get_value()];
    }
    return res;
}

BDD SymVariables::getStateBDD(const std::vector<int> &state) const {
    BDD res = _manager->bddOne();
    for (int i = var_order.size() - 1; i >= 0; i--) {
        res = res * preconditionBDDs[var_order[i]][state[var_order[i]]];
    }
    return res;
}

BDD SymVariables::getPartialStateBDD(const std::vector<std::pair<int, int>> &state) const {
    BDD res = validBDD;
    for (int i = state.size() - 1; i >= 0; i--) {
        res = res * preconditionBDDs[state[i].first][state[i].second];
    }
    return res;
}

bool SymVariables::isIn(const State &state, const BDD &bdd) const {
    BDD sBDD = getStateBDD(state);
    return !((sBDD * bdd).IsZero());
}

double SymVariables::numStates(const BDD &bdd) const {
    return bdd.CountMinterm(numBDDVars);
}

double SymVariables::numStates() const {
    return numStates(validBDD);
}

void SymVariables::writeBucket(const std::string &fname,
                               std::ofstream &filenames,
                               const std::vector<BDD> &bucket) {
    for (unsigned int i = 0; i < bucket.size(); ++i) {
        std::stringstream file;
        file << fname << "_" << i;
        bucket[i].write(file.str());
        filenames << file.str() << std::endl;
    }
    filenames << std::endl;
}

void SymVariables::readBucket(std::ifstream &filenames, std::vector<BDD> &bucket) const {
    std::string line;
    while (getline(filenames, line) && !line.empty()) {
        bucket.push_back(_manager->read_file(line));
    }
}


void SymVariables::writeMapBucket(const std::string &fname,
                                  std::ofstream &filenames,
                                  const std::map<int, std::vector<BDD>> &mb) {
    for (const auto &entry: mb) {
        filenames << entry.first << std::endl;
        std::stringstream file;
        file << fname << entry.first;
        writeBucket(file.str(), filenames, entry.second);
    }
    filenames << -1 << std::endl;
}

void SymVariables::readMapBucket(std::ifstream &filenames, std::map<int, std::vector<BDD>> &data) const {
    int key = getData<int>(filenames, "");
    while (key != -1) {
        std::vector<BDD> bucket;
        readBucket(filenames, bucket);
        data[key] = bucket;
        key = getData<int>(filenames, "");
    }
}


void SymVariables::writeMap(const std::string &fname,
                            std::ofstream &filenames,
                            const std::map<int, BDD> &m) {
    for (const auto &entry: m) {
        filenames << entry.first << std::endl;
        std::stringstream file;
        file << fname << entry.first;
        filenames << file.str() << std::endl;
        entry.second.write(file.str());
    }
    filenames << -1 << std::endl;
}

void SymVariables::readMap(std::ifstream &filenames,
                           std::map<int, BDD> &data) const {
    int key = getData<int>(filenames, "");
    std::string filename;
    while (key != -1) {
        getline(filenames, filename);
        data[key] = readBDD(filename);
        key = getData<int>(filenames, "");
    }
}

BDD SymVariables::readBDD(const std::string &filename) const {
    std::cout << "Read BDD: " << filename << std::endl;
    return _manager->read_file(filename);
}

BDD SymVariables::generateBDDVar(const std::vector<int> &_bddVars, int value) const {
    BDD res = _manager->bddOne();
    for (int _bddVar : _bddVars) {
        if (value % 2) {     //Check if the binary variable is asserted or negated
            res = res * variables[_bddVar];
        } else {
            res = res * (!variables[_bddVar]);
        }
        value /= 2;
    }
    return res;
}

BDD SymVariables::createBiimplicationBDD(const std::vector<int> &vars, const std::vector<int> &vars2) const {
    BDD res = _manager->bddOne();
    for (unsigned int i = 0; i < vars.size(); i++) {
        res *= variables[vars[i]].Xnor(variables[vars2[i]]);
    }
    return res;
}

std::vector<BDD>
SymVariables::getBDDVars(const std::vector<int> &vars, const std::vector<std::vector<int>> &v_index) const {
    std::vector<BDD> res;
    for (int v: vars) {
        for (int bddv: v_index[v]) {
            res.push_back(variables[bddv]);
        }
    }
    return res;
}


BDD SymVariables::getCube(int var, const std::vector<std::vector<int>> &v_index) const {
    BDD res = oneBDD();
    for (int bddv: v_index[var]) {
        res *= variables[bddv];
    }
    return res;
}

BDD SymVariables::getCube(const std::set<int> &vars, const std::vector<std::vector<int>> &v_index) const {
    BDD res = oneBDD();
    for (int v: vars) {
        for (int bddv: v_index[v]) {
            res *= variables[bddv];
        }
    }
    return res;
}


void
exceptionError(std::string /*message*/) {
    //cout << message << endl;
    throw BDDError();
}


void SymVariables::print() {
    std::ofstream file("variables.txt");

    for (int v: var_order) {
        file << "vars: ";
        for (int j: bdd_index_pre[v])
            std::cout << j << " ";
        file << std::endl;
        for (int val = 0; val < global_simulation_task()->get_variable_domain_size(v); ++val) {
            file << global_simulation_task()->get_fact_name(FactPair(v, val)) << std::endl;
        }
    }
}
}
