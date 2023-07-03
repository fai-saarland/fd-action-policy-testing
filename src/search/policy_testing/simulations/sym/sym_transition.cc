#include "sym_transition.h"
#include "../utils/debug.h"
#include "sym_abstraction.h"
#include "sym_manager.h"
#include "sym_pdb.h"
#include "sym_smas.h"

//#include "../timer.h"
#include "../merge_and_shrink/simulation_relation.h"
#include "../merge_and_shrink/dominance_relation.h"

namespace simulations {
SymTransition::SymTransition(SymManager *mgr,
                             const DominanceRelation &dominance_relation) :
    sV(mgr->getVars()), cost(0), tBDD(mgr->getVars()->oneBDD()),
    existsVars(mgr->getVars()->oneBDD()), existsBwVars(mgr->getVars()->oneBDD()),
    absAfterImage(nullptr) {
    const std::vector<std::unique_ptr<SimulationRelation>> &simulations = dominance_relation.get_simulations();

    // a) Collect effect variables
    for (auto &sim: simulations) {
        const std::vector<int> &vset = sim->get_varset();
        effVars.insert(effVars.end(), vset.begin(), vset.end());
    }


    // b) Initialize swapVars
    sort(effVars.begin(), effVars.end());
    for (int var: effVars) {
        for (int bdd_var: sV->vars_index_pre(var)) {
            swapVarsS.push_back(sV->bddVar(bdd_var));
        }
        for (int bdd_var: sV->vars_index_eff(var)) {
            swapVarsSp.push_back(sV->bddVar(bdd_var));
        }
    }

    // c) existsVars/existsBwVars is just the conjunction of swapVarsS and swapVarsSp
    for (int i = 0; i < swapVarsS.size(); ++i) {
        existsVars *= swapVarsS[i];
        existsBwVars *= swapVarsSp[i];
    }
    // d) Compute tBDD
    for (auto it = simulations.rbegin(); it != simulations.rend(); ++it) {
        const std::vector<BDD> &dominated_bdds = (*it)->get_dominated_bdds();
        const std::vector<BDD> &abs_bdds = (*it)->get_abs_bdds();

        BDD simBDD = sV->zeroBDD();
        //    BDD totalAbsBDDs = sV->zeroBDD();
        for (int i = 0; i < abs_bdds.size(); i++) {
            BDD dom = dominated_bdds[i];
            try {
                // TODO mutexes not supported yet
                // dom = mgr->filter_mutex(dom, true, 1000000, true);
                // dom = mgr->filter_mutex(dom, false, 1000000, true);
            } catch (BDDError &e) {
                std::cout << "Simulation TR error: " << std::endl;
            }
            simBDD += (abs_bdds[i] * dom.SwapVariables(swapVarsS, swapVarsSp));
            //totalAbsBDDs += abs_bdds[i];
        }
        tBDD *= simBDD;
    }
    try {
        // TODO mutexes not supported yet
        /*std::cout << "Simulation TR size before filtering mutex: " << tBDD.nodeCount() << std::endl;
        tBDD = mgr->filter_mutex(tBDD, true, 1000000, true);
        std::cout << "Simulation TR size in the middle of filtering mutex: " << tBDD.nodeCount() << std::endl;
        tBDD = mgr->filter_mutex(tBDD, false, 1000000, true);
        std::cout << "Simulation TR size after filtering mutex: " << tBDD.nodeCount() << std::endl; */
    } catch (BDDError &e) {
        std::cout << "Simulation TR mutex could not be filtered: " << tBDD.nodeCount() << std::endl;
    }
}

SymTransition::SymTransition(SymVariables *sVars, const OperatorID op, int cost_) :
    sV(sVars),
    cost(cost_), tBDD(sVars->oneBDD()),
    existsVars(sVars->oneBDD()), existsBwVars(sVars->oneBDD()),
    absAfterImage(nullptr) {
    ops.insert(op);
    for (auto prevail : get_prevails(op)) {     //Put precondition of label
        tBDD *= sV->preBDD(prevail.var, prevail.prev);
    }

    std::map<int, BDD> effect_conditions;
    std::map<int, BDD> effects;
    // Get effects and the remaining conditions. We iterate in reverse
    // order because pre_post at the end have preference
    for (int i = get_preposts(op).size() - 1; i >= 0; i--) {
        const PrePost &pre_post = get_preposts(op)[i];
        int var = pre_post.var;
        if (std::find(std::begin(effVars), std::end(effVars), var) == std::end(effVars)) {
            effVars.push_back(var);
        }

        BDD condition = sV->oneBDD();
        BDD ppBDD = sV->effBDD(var, pre_post.post);
        if (effect_conditions.count(var)) {
            condition = effect_conditions.at(var);
        } else {
            effect_conditions[var] = condition;
            effects[var] = sV->zeroBDD();
        }

        for (const auto &cPrev: pre_post.cond) {
            condition *= sV->preBDD(cPrev.var, cPrev.prev);
        }
        effect_conditions[var] *= !condition;
        effects[var] += (condition * ppBDD);
        //Add precondition to the tBDD
        if (pre_post.pre != -1) {
            tBDD *= sV->preBDD(pre_post.var, pre_post.pre);
        }
    }

    //Add effects to the tBDD
    for (auto it = effects.rbegin(); it != effects.rend(); ++it) {
        int var = it->first;
        BDD effectBDD = it->second;
        //If some possibility is not covered by the conditions of the
        //conditional effect, then in those cases the value of the value
        //is preserved with a biimplication
        if (!effect_conditions[var].IsZero()) {
            effectBDD += (effect_conditions[var] * sV->biimp(var));
        }
        tBDD *= effectBDD;
    }
    if (tBDD.IsZero()) {
        std::cerr << "ERROR, DESAMBIGUACION: " << get_op_proxy(op).get_name() << std::endl;
        //exit(0);
    }

    sort(effVars.begin(), effVars.end());
    for (int var: effVars) {
        for (int bdd_var: sV->vars_index_pre(var)) {
            swapVarsS.push_back(sV->bddVar(bdd_var));
        }
        for (int bdd_var: sV->vars_index_eff(var)) {
            swapVarsSp.push_back(sV->bddVar(bdd_var));
        }
    }
    assert(swapVarsS.size() == swapVarsSp.size());
    // existsVars/existsBwVars is just the conjunction of swapVarsS and swapVarsSp
    for (int i = 0; i < swapVarsS.size(); ++i) {
        existsVars *= swapVarsS[i];
        existsBwVars *= swapVarsSp[i];
    }
    //DEBUG_MSG(std::cout << "Computing tr took " << tr_timer; tBDD.print(1, 1););
}

void SymTransition::shrink(const SymAbstraction &abs, int maxNodes) {
    tBDD = abs.shrinkTBDD(tBDD, maxNodes);

    // effVars
    std::vector<int> newEffVars;
    for (int var: effVars) {
        if (abs.isRelevantVar(var)) {
            newEffVars.push_back(var);
        }
    }
    newEffVars.swap(effVars);
}

bool SymTransition::setMaSAbstraction(SymAbstraction */* abs*/,
                                      const BDD &bddSrc,
                                      const BDD &bddTarget) {
    tBDD *= bddSrc;
    tBDD *= bddTarget;
    return true;
}

BDD SymTransition::image(const BDD &from) const {
    BDD aux = from;
    if (!swapVarsA.empty()) {
        aux = from.SwapVariables(swapVarsA, swapVarsAp);
    }
    BDD tmp = tBDD.AndAbstract(aux, existsVars);
    BDD res = tmp.SwapVariables(swapVarsS, swapVarsSp);
    if (absAfterImage) {
        //TODO: HACK: PARAMETER FIXED
        res = absAfterImage->shrinkExists(res, 10000000);
    }
    return res;
}

BDD SymTransition::image(const BDD &from, int maxNodes) const {
    DEBUG_MSG(std::cout << "Image cost " << cost << " from " << from.nodeCount() << " with " << tBDD.nodeCount();
              );
    BDD aux = from;
    if (!swapVarsA.empty()) {
        aux = from.SwapVariables(swapVarsA, swapVarsAp);
    }
    utils::Timer t;
    BDD tmp = tBDD.AndAbstract(aux, existsVars, maxNodes);
    DEBUG_MSG(std::cout << " tmp " << tmp.nodeCount() << " in " << t();
              );
    BDD res = tmp.SwapVariables(swapVarsS, swapVarsSp);
    DEBUG_MSG(std::cout << " res " << res.nodeCount() << " took " << t();
              );
    if (absAfterImage) {
        res = absAfterImage->shrinkExists(res, maxNodes);
        DEBUG_MSG(std::cout << " shrunk: " << res.nodeCount() << " tookTotal: " << t();
                  );
    }
    DEBUG_MSG(std::cout << std::endl;
              );

    return res;
}

BDD SymTransition::preimage(const BDD &from) const {
    BDD tmp = from.SwapVariables(swapVarsS, swapVarsSp);
    BDD res = tBDD.AndAbstract(tmp, existsBwVars);
    if (!swapVarsA.empty()) {
        res = res.SwapVariables(swapVarsA, swapVarsAp);
    }
    if (absAfterImage) {
        res = absAfterImage->shrinkExists(res, std::numeric_limits<int>::max());
    }
    return res;
}

BDD SymTransition::preimage(const BDD &from, int maxNodes) const {
    utils::Timer t;
    DEBUG_MSG(std::cout << "Image cost " << cost << " from " << from.nodeCount() << " with " << tBDD.nodeCount()
                        << std::flush;
              );
    BDD tmp = from.SwapVariables(swapVarsS, swapVarsSp);
    DEBUG_MSG(std::cout << " tmp " << tmp.nodeCount() << " in " << t() << std::flush;
              );
    BDD res = tBDD.AndAbstract(tmp, existsBwVars, maxNodes);
    if (!swapVarsA.empty()) {
        res = res.SwapVariables(swapVarsA, swapVarsAp);
    }
    DEBUG_MSG(std::cout << "res " << res.nodeCount() << " took " << t();
              );
    if (absAfterImage) {
        res = absAfterImage->shrinkExists(res, maxNodes);
        DEBUG_MSG(std::cout << " shrunk: " << res.nodeCount() << " tookTotal: " << t();
                  );
    }
    DEBUG_MSG(std::cout << std::endl;
              );

    return res;
}

void SymTransition::merge(const SymTransition &t2,
                          int maxNodes) {
    assert(cost == t2.cost);
    if (cost != t2.cost) {
        std::cout << "Error: merging transitions with different cost: " << cost << " " << t2.cost << std::endl;
        exit(-1);
    }

    //Attempt to generate the new tBDD
    std:: vector<int> newEffVars;
    set_union(effVars.begin(), effVars.end(),
              t2.effVars.begin(), t2.effVars.end(),
              back_inserter(newEffVars));

    BDD newTBDD = tBDD;
    BDD newTBDD2 = t2.tBDD;

    //    std::cout << "Eff vars" << std::endl;
    auto var1 = effVars.begin();
    auto var2 = t2.effVars.begin();
    for (int newEffVar : newEffVars) {
        if (var1 == effVars.end() || *var1 != newEffVar) {
            BDD tmp = sV->biimp(newEffVar);
            newTBDD = newTBDD * tmp;
        } else {
            ++var1;
        }

        if (var2 == t2.effVars.end() || *var2 != newEffVar) {
            //cout << "a2" << newTBDD.getNode() << std::endl;
            BDD tmp = sV->biimp(newEffVar);
            //      std::cout << "a2 " << newTBDD.getNode() << " and " << tmp.getNode() << std::endl;
            newTBDD2 = newTBDD2 * tmp;
            //      std::cout << "done." << std::endl;
        } else {
            ++var2;
        }
    }
    newTBDD = newTBDD.Or(newTBDD2, maxNodes);

    if (newTBDD.nodeCount() > maxNodes) {
        DEBUG_MSG(std::cout << "TR size exceeded: " << newTBDD.nodeCount() <<
                  ">" << maxNodes << std::endl;
                  );
        throw BDDError();
    }

    tBDD = newTBDD;


    /*cout << "Eff vars: ";
    for(int i = 0; i < effVars.size(); i++) std::cout << effVars[i] << " ";
    std::cout << std::endl;*/

    effVars.swap(newEffVars);

    /*  std::cout << "New eff vars: ";
    for(int i = 0; i < effVars.size(); i++) std::cout << effVars[i] << " ";
    std::cout << std::endl;*/

    /*cout << "EXIST BW VARS: " << std::endl;
    existsBwVars.print(1, 2);
    t2.existsBwVars.print(1, 2);*/
    existsVars *= t2.existsVars;
    existsBwVars *= t2.existsBwVars;

    /*  existsBwVars.print(1, 2);

    std::cout << "Swap vars" << std::endl;
    for(int i = 0; i < swapVarsS.size(); i++){
      std::cout << "Swap "; swapVarsS[i].PrintMinterm();
      std::cout << "with "; swapVarsSp[i].PrintMinterm();
    }

    std::cout << "Swap vars 2" << std::endl;
    for(int i = 0; i < swapVarsS.size(); i++){
      std::cout << "Swap "; swapVarsS[i].PrintMinterm();
      std::cout << "with "; swapVarsSp[i].PrintMinterm();
    }*/



    for (int i = 0; i < t2.swapVarsS.size(); i++) {
        if (find(swapVarsS.begin(), swapVarsS.end(), t2.swapVarsS[i]) ==
            swapVarsS.end()) {
            swapVarsS.push_back(t2.swapVarsS[i]);
            swapVarsSp.push_back(t2.swapVarsSp[i]);
        }
    }

    ops.insert(t2.ops.begin(), t2.ops.end());
}

/*
    void SymTransition::edeletion(SymManager &mgr) {
        if (ops.size() != 1) {
            std::cerr << "Error, trying to apply edeletion over a transition with more than one op" << std::endl;
            exit(-1);
        }

        //For each op, include relevant mutexes
        for (OperatorID op: ops) {
            for (const PrePost &pp: get_preposts(op)) {
                //edeletion bw
                if (pp.pre == -1) {
                    //We have a post effect over this variable.
                    //That means that every previous value is possible
                    //for each value of the variable
                    for (int val = 0; val < global_simulation_task()->get_variable_domain_size(pp.var); val++) {
                        tBDD *= mgr.getNotMutexBDDBw(pp.var, val);
                    }
                } else {
                    //In regression, we are making true pp.pre
                    //So we must negate everything of these.
                    tBDD *= mgr.getNotMutexBDDBw(pp.var, pp.pre);
                }
                //edeletion fw
                tBDD *= mgr.getNotMutexBDDFw(pp.var, pp.post).SwapVariables(swapVarsS, swapVarsSp);

                //edeletion invariants
                tBDD *= mgr.getExactlyOneBDD(pp.var, pp.post);
            }
        }
    }
    */


std::ostream &operator<<(std::ostream &os, const SymTransition &tr) {
    os << "TR(";
    for (auto op: tr.ops) {
        os << get_op_proxy(op).get_name() << ", ";
    }
    return os << "): " << tr.tBDD.nodeCount() << std::endl;
}
}
