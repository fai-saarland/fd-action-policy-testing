#include "sym_manager.h"

#include "sym_enums.h"
#include "../utils/debug.h"
#include <queue>
#include "sym_abstraction.h"
#include "sym_util.h"
#include "sym_test_mutex.h"
#include "../merge_and_shrink/ld_simulation.h"
#include "sym_prune_heuristic.h"

#include "../../testing_environment.h"


namespace simulations {
SymManager::SymManager(SymVariables *v,
                       SymAbstraction *abs,
                       const SymParamsMgr &params,
                       OperatorCost cost_type_) : vars(v), abstraction(abs), p(params),
                                                  cost_type(cost_type_), parentMgr(nullptr),
                                                  initialState(v->zeroBDD()),
                                                  goal(v->zeroBDD()),
                                                  min_transition_cost(0), hasTR0(false),
                                                  // mutexInitialized(false), TODO support mutexes
                                                  // mutexByFluentInitialized(false), TODO support mutexes
                                                  prune_heuristic(nullptr) {
    for (const auto &op: global_simulation_task_proxy()->get_operators()) {
        if (is_dead(op.get_id()))
            continue;

        if (min_transition_cost == 0 ||
            min_transition_cost > get_adjusted_action_cost(op, cost_type, has_unit_cost())) {
            min_transition_cost = get_adjusted_action_cost(op, cost_type, has_unit_cost());
        }
        if (get_adjusted_action_cost(op, cost_type, has_unit_cost()) == 0) {
            hasTR0 = true;
        }
    }
}

SymManager::SymManager(SymManager *mgr,
                       SymAbstraction *abs,
                       const SymParamsMgr &params) : vars(mgr->getVars()), abstraction(abs), p(params),
                                                     cost_type(mgr->cost_type), parentMgr(mgr),
                                                     initialState(mgr->zeroBDD()),
                                                     goal(mgr->zeroBDD()),
                                                     min_transition_cost(0), hasTR0(false),
                                                     // mutexInitialized(false), TODO support mutexes
                                                     // mutexByFluentInitialized(false), TODO support mutexes
                                                     prune_heuristic(nullptr) {
    if (mgr) {
        min_transition_cost = mgr->getMinTransitionCost();
        hasTR0 = mgr->hasTransitions0();
    } else {
        for (const auto &op: global_simulation_task_proxy()->get_operators()) {
            if (is_dead(op.get_id()))
                continue;

            if (min_transition_cost == 0 ||
                min_transition_cost > get_adjusted_action_cost(op, cost_type, has_unit_cost())) {
                min_transition_cost = get_adjusted_action_cost(op, cost_type, has_unit_cost());
            }
            if (get_adjusted_action_cost(op, cost_type, has_unit_cost()) == 0) {
                hasTR0 = true;
            }
        }
    }
}

void SymManager::init_states() {
    DEBUG_MSG(std::cout << "INIT STATES" << std::endl;
              );
    if (abstraction && abstraction->isAbstracted()) {
        DEBUG_MSG(std::cout << "INIT STATES ABS" << std::endl;
                  );
        initialState = abstraction->getInitialState();
        goal = abstraction->getGoal();
    } else {
        DEBUG_MSG(std::cout << "INIT STATES NO ABS" << std::endl;
                  );
        initialState = vars->getStateBDD(global_simulation_task()->get_initial_state_values());
        std::vector<std::pair<int, int>> goal_facts;
        for (const auto &goal_fact : global_simulation_task_proxy()->get_goals()) {
            goal_facts.emplace_back(goal_fact.get_variable().get_id(), goal_fact.get_value());
        }
        goal = vars->getPartialStateBDD(goal_facts);
    }
    DEBUG_MSG(std::cout << "INIT STATES DONE" << std::endl;
              );
}

//TODO: WARNING INIT_MUTEX with abstractions could be better.
// TODO support mutexes
/*
    void SymManager::init_mutex(const std::vector<MutexGroup> &mutex_groups) {
        //If (a) is initialized OR not using mutex OR edeletion does not need mutex
        if (mutexInitialized || p.mutex_type == MutexType::MUTEX_NOT)
            return; //Skip mutex initialization

        //Do I really need
        if (parentMgr && abstraction && abstraction->isAbstracted()) {
            setTimeLimit(p.max_mutex_time);
            DEBUG_MSG(std::cout << "Init mutex from parent" << std::endl;);
            mutexInitialized = true;
            //Initialize mutexes from other manager
            try {
                for (auto &bdd: parentMgr->notMutexBDDsFw) {
                    BDD shrinked = abstraction->shrinkExists(bdd, p.max_mutex_size);
                    notMutexBDDsFw.push_back(shrinked);
                }
                for (auto &bdd: parentMgr->notMutexBDDsBw) {
                    BDD shrinked = abstraction->shrinkExists(bdd, p.max_mutex_size);
                    notMutexBDDsBw.push_back(shrinked);
                }
                unsetTimeLimit();
            } catch (BDDError &e) {
                unsetTimeLimit();
                //Forget about it
                std::vector<BDD>().swap(notMutexBDDsFw);
                std::vector<BDD>().swap(notMutexBDDsBw);
                init_mutex(mutex_groups, true, false);
            }
            //We will compute mutex by fluent on demand
        } else {
            if (p.mutex_type == MutexType::MUTEX_EDELETION) {
                init_mutex(mutex_groups, true, true);
            } else {
                init_mutex(mutex_groups, true, false);
            }
        }
    }

    void SymManager::init_mutex(const std::vector<MutexGroup> &mutex_groups,
                                bool genMutexBDD, bool genMutexBDDByFluent) {

        //Check if I should initialize something and return
        if (mutexInitialized) genMutexBDD = false;
        if (mutexByFluentInitialized) genMutexBDDByFluent = false;
        if (!genMutexBDD && !genMutexBDDByFluent)
            return;
        if (genMutexBDD) mutexInitialized = true;
        if (genMutexBDDByFluent) mutexByFluentInitialized = true;

        if (genMutexBDDByFluent) {
            //Initialize structure for exactlyOneBDDsByFluent (common to both init_mutex calls)
            const int num_variables = global_simulation_task()->get_num_variables();
            exactlyOneBDDsByFluent.resize(num_variables);
            for (size_t i = 0; i < num_variables; ++i) {
                const int domain_size = global_simulation_task()->get_variable_domain_size(i);
                exactlyOneBDDsByFluent[i].resize(domain_size);
                for (size_t j = 0; j < domain_size; ++j) {
                    exactlyOneBDDsByFluent[i][j] = oneBDD();
                }
            }
        }
        init_mutex(mutex_groups, genMutexBDD, genMutexBDDByFluent, false);
        init_mutex(mutex_groups, genMutexBDD, genMutexBDDByFluent, true);
    }
    void SymManager::init_mutex(const std::vector<MutexGroup> &mutex_groups,
                                bool genMutexBDD, bool genMutexBDDByFluent, bool fw) {
        DEBUG_MSG(std::cout << "Init mutex BDDs " << (fw ? "fw" : "bw") << ": "
                       << genMutexBDD << " " << genMutexBDDByFluent << std::endl;);

        std::vector<std::vector<BDD>> &notMutexBDDsByFluent =
                (fw ? notMutexBDDsByFluentFw : notMutexBDDsByFluentBw);

        std::vector<BDD> &notMutexBDDs =
                (fw ? notMutexBDDsFw : notMutexBDDsBw);

        //BDD validStates = vars->oneBDD();
        int num_mutex = 0;
        int num_invariants = 0;

        const int num_variables = global_simulation_task()->get_num_variables();

        if (genMutexBDDByFluent) {
            //Initialize structure for notMutexBDDsByFluent
            notMutexBDDsByFluent.resize(num_variables);
            for (size_t i = 0; i < num_variables; ++i) {
                const int domain_size = global_simulation_task()->get_variable_domain_size(i);
                notMutexBDDsByFluent[i].resize(domain_size);
                for (size_t j = 0; j < domain_size; ++j) {
                    notMutexBDDsByFluent[i][j] = oneBDD();
                }
            }
        }

        //Initialize mBDDByVar and invariant_bdds_by_fluent
        std::vector<BDD> mBDDByVar;
        mBDDByVar.reserve(num_variables);
        std::vector<std::vector<BDD>> invariant_bdds_by_fluent(num_variables);
        for (int i = 0; i < invariant_bdds_by_fluent.size(); i++) {
            mBDDByVar.push_back(oneBDD());
            invariant_bdds_by_fluent[i].resize(global_simulation_task()->get_variable_domain_size(i));
            for (auto & j : invariant_bdds_by_fluent[i]) {
                j = oneBDD();
            }
        }

        for (auto &mg: mutex_groups) {
            if (mg.pruneFW() != fw)
                continue;
            const std::vector<std::pair<int, int> > &invariant_group = mg.getFacts();
            DEBUG_MSG(std::cout << mg << std::endl;);
            if (mg.isExactlyOne()) {
                BDD bddInvariant = zeroBDD();
                int var = std::numeric_limits<int>::max();
                int val = 0;
                bool exactlyOneRelevant = true;

                for (auto &fluent: invariant_group) {
                    if (abstraction && !abstraction->isRelevantVar(fluent.first)) {
                        exactlyOneRelevant = true;
                        break;
                    }
                    bddInvariant += vars->preBDD(fluent.first, fluent.second);
                    if (fluent.first < var) {
                        var = fluent.first;
                        val = fluent.second;
                    }
                }

                if (exactlyOneRelevant) {
                    num_invariants++;
                    if (genMutexBDD) {
                        invariant_bdds_by_fluent[var][val] *= bddInvariant;
                    }
                    if (genMutexBDDByFluent) {
                        for (auto &fluent: invariant_group) {
                            exactlyOneBDDsByFluent[fluent.first][fluent.second] *= bddInvariant;
                        }
                    }
                }
            }

            for (int i = 0; i < invariant_group.size(); ++i) {
                int var1 = invariant_group[i].first;
                if (abstraction && !abstraction->isRelevantVar(var1)) continue;
                int val1 = invariant_group[i].second;
                BDD f1 = vars->preBDD(var1, val1);

                for (int j = i + 1; j < invariant_group.size(); ++j) {
                    int var2 = invariant_group[j].first;
                    if (abstraction && !abstraction->isRelevantVar(var2)) continue;
                    int val2 = invariant_group[j].second;
                    BDD f2 = vars->preBDD(var2, val2);
                    BDD mBDD = !(f1 * f2);
                    if (genMutexBDD) {
                        num_mutex++;
                        mBDDByVar[std::min(var1, var2)] *= mBDD;
                        if (mBDDByVar[std::min(var1, var2)].nodeCount() > p.max_mutex_size) {
                            notMutexBDDs.push_back(mBDDByVar[std::min(var1, var2)]);
                            mBDDByVar[std::min(var1, var2)] = vars->oneBDD();
                        }
                    }
                    if (genMutexBDDByFluent) {
                        notMutexBDDsByFluent[var1][val1] *= mBDD;
                        notMutexBDDsByFluent[var2][val2] *= mBDD;
                    }
                }
            }
        }

        if (genMutexBDD) {
            for (int var = 0; var < num_variables; ++var) {
                if (!mBDDByVar[var].IsOne()) {
                    notMutexBDDs.push_back(mBDDByVar[var]);
                }
                for (const BDD &bdd_inv: invariant_bdds_by_fluent[var]) {
                    if (!bdd_inv.IsOne()) {
                        notMutexBDDs.push_back(bdd_inv);
                    }
                }
            }

            DEBUG_MSG(dumpMutexBDDs(fw););
            merge(vars, notMutexBDDs, mergeAndBDD,
                  p.max_mutex_time, p.max_mutex_size);
            std::reverse(std::begin(notMutexBDDs), std::end(notMutexBDDs));
            DEBUG_MSG(std::cout << "Mutex initialized " << (fw ? "fw" : "bw") << ". Total mutex added: " << num_mutex
                           << " Invariant groups: " << num_invariants << std::endl;);
            dumpMutexBDDs(fw);
        }
    }

*/
void SymManager::addDeadEndStates(bool fw, BDD bdd) {
    //There are several options here, we could follow with edeletion
    //and modify the TRs, so that the new spurious states are never
    //generated. However, the TRs are already merged and the may get
    //too large. Therefore we just keep this states in another vectors
    //and spurious states are always removed. TODO: this could be
    //improved.
    if (fw || abstraction) {
        if (abstraction)
            bdd = shrinkForall(bdd);
        notDeadEndFw.push_back(!bdd);
        mergeBucketAnd(notDeadEndFw);
    } else {
        notDeadEndBw.push_back(!bdd);
        mergeBucketAnd(notDeadEndBw);
    }
}


void SymManager::addDeadEndStates(const std::vector<BDD> &fw_dead_ends,
                                  const std::vector<BDD> &bw_dead_ends) {
    if (abstraction) {
        for (BDD bdd: fw_dead_ends) {
            bdd = shrinkForall(bdd);
            if (!bdd.IsZero()) {
                notDeadEndFw.push_back(!bdd);
            }
        }

        for (BDD bdd: bw_dead_ends) {
            bdd = shrinkForall(bdd);
            if (!bdd.IsZero()) {
                notDeadEndFw.push_back(!bdd);
            }
        }
        mergeBucketAnd(notDeadEndFw);
    } else {
        for (const BDD &bdd: fw_dead_ends) {
            if (!bdd.IsZero()) {
                notDeadEndFw.push_back(!bdd);
            }
        }
        mergeBucketAnd(notDeadEndFw);


        for (const BDD &bdd: bw_dead_ends) {
            if (!bdd.IsZero()) {
                notDeadEndBw.push_back(!bdd);
            }
        }
        mergeBucketAnd(notDeadEndBw);
    }
}

/*
    void SymManager::dumpMutexBDDs(bool fw) const {
        if (fw) {
            std::cout << "Mutex BDD FW Size(" << p.max_mutex_size << "):";
            for (const auto &bdd: notMutexBDDsFw) {
                std::cout << " " << bdd.nodeCount();
            }
            std::cout << std::endl;
        } else {

            std::cout << "Mutex BDD BW Size(" << p.max_mutex_size << "):";
            for (const auto &bdd: notMutexBDDsBw) {
                std::cout << " " << bdd.nodeCount();
            }
            std::cout << std::endl;
        }

    }
*/

//TODO: Shrink directly the merged transitions
const std::map<int, std::vector<SymTransition>> &SymManager::getIndividualTRs() {
    if (indTRs.empty()) {
        if (abstraction && abstraction->isAbstracted() && parentMgr) {
            std::cout << "Initialize individual TRs of " << *abstraction << " IS NOT IMPLEMENTED!!" << std::endl;
            exit(-1);
        } else {
            DEBUG_MSG(std::cout << "Initialize individual TRs of original state space" << std::endl;
                      );
            for (int i = 0; i < global_simulation_task()->get_num_operators(); i++) {
                auto op = global_simulation_task_proxy()->get_operators()[i];
                // Skip irrelevant operators
                if (is_dead(op.get_id())) {
                    continue;
                }
                int cost = get_adjusted_action_cost(op, cost_type, has_unit_cost());
                DEBUG_MSG(std::cout << "Creating TR of op " << i << " of cost " << cost << std::endl;
                          );
                indTRs[cost].push_back(SymTransition(vars, OperatorID(op.get_id()), cost));
                if (p.mutex_type == MutexType::MUTEX_EDELETION) {
                    // TODO support mutexes
                    // indTRs[cost].back().edeletion(*this);
                }
            }
        }
    }
    return indTRs;
}

void SymManager::init_transitions() {
    if (!transitions.empty())
        return;                           //Already initialized!

    DEBUG_MSG(std::cout << "Init transitions" << std::endl;
              );

    if (abstraction && abstraction->isAbstracted() && parentMgr) {
        DEBUG_MSG(std::cout << "Init transitions from parent state space" << std::endl;
                  );
        SymManager *mgr_parent = parentMgr;
        while (mgr_parent && mgr_parent->transitions.empty()) {
            mgr_parent = mgr_parent->parentMgr;
        }
        const auto &trsParent = mgr_parent->getTransitions();

        while (mgr_parent && mgr_parent->indTRs.empty()) {
            mgr_parent = mgr_parent->parentMgr;
        }
        const auto &indTRsParent = mgr_parent->getIndividualTransitions();

        abstraction->shrinkTransitions(trsParent, indTRsParent,
                                       transitions,
                                       p.max_tr_time, p.max_tr_size);
        return;
    }

    DEBUG_MSG(std::cout << "Generate individual TRs" << std::endl;
              );
    transitions = std::map<int, std::vector<SymTransition>>(getIndividualTRs());     //Copy
    DEBUG_MSG(std::cout << "Individual TRs generated" << std::endl;
              );
    min_transition_cost = 0;
    hasTR0 = transitions.count(0) > 0;

    for (auto &transition : transitions) {
        merge(vars, transition.second, mergeTR, p.max_tr_time, p.max_tr_size);

        if (min_transition_cost == 0 || min_transition_cost > transition.first) {
            min_transition_cost = transition.first;
        }
        // PIET-edit: commented out for (better) readability of the output
        std::cout << "TRs cost=" << transition.first << " (" << transition.second.size() << ")" << std::endl;
    }
}

void SymManager::zero_preimage(const BDD &bdd, std::vector <BDD> &res, int nodeLimit) const {
    for (unsigned int i = res.size(); i < transitions.at(0).size(); i++) {
        res.push_back(transitions.at(0)[i].preimage(bdd, nodeLimit));
    }
}

void SymManager::zero_image(const BDD &bdd, std::vector <BDD> &res, int nodeLimit) const {
    for (unsigned int i = res.size(); i < transitions.at(0).size(); i++) {
        res.push_back(transitions.at(0)[i].image(bdd, nodeLimit));
    }
}


void SymManager::cost_preimage(const BDD &bdd, std::map<int, std::vector<BDD>> &res,
                               int nodeLimit) const {
    for (auto trs: transitions) {
        int cost = trs.first;
        if (cost == 0)
            continue;
        for (int i = res[cost].size(); i < trs.second.size(); i++) {
            BDD result = trs.second[i].preimage(bdd, nodeLimit);
            res[cost].push_back(result);
        }
    }
}

void SymManager::cost_image(const BDD &bdd,
                            std::map<int, std::vector<BDD>> &res,
                            int nodeLimit) const {
    for (auto trs: transitions) {
        int cost = trs.first;
        if (cost == 0)
            continue;
        for (int i = res[cost].size(); i < trs.second.size(); i++) {
            //cout << "Img: " << trs.second[i].nodeCount() << " with bdd " << bdd.nodeCount() << " node limit: " << nodeLimit << endl;
            BDD result = trs.second[i].image(bdd, nodeLimit);
            //cout << "Res: " << result.nodeCount() << endl;
            res[cost].push_back(result);
        }
    }
}

/*
    BDD SymManager::filter_mutex(const BDD &bdd, bool fw,
                                 int nodeLimit, bool initialization) {
        BDD res = bdd;
        //if(fw) return bdd; //Only filter mutex in backward direction
        const std::vector<BDD> &notDeadEndBDDs = ((fw || abstraction) ? notDeadEndFw : notDeadEndBw);
        for (const BDD &notDeadEnd: notDeadEndBDDs) {
            DEBUG_MSG(std::cout << "Filter: " << res.nodeCount() << " and dead end " << notDeadEnd.nodeCount() << std::flush;);
            //cout << "Filter: " << res.nodeCount()  << " and dead end " <<  notDeadEnd.nodeCount()  << flush;
            res = res.And(notDeadEnd, nodeLimit);
            //cout << ": " << res.nodeCount() << endl;
            DEBUG_MSG(std::cout << ": " << res.nodeCount() << std::endl;);
        }

        const std::vector<BDD> &notMutexBDDs = (fw ? notMutexBDDsFw : notMutexBDDsBw);


        switch (p.mutex_type) {
            case MutexType::MUTEX_NOT:
                break;
            case MutexType::MUTEX_EDELETION:
                if (initialization) {
                    for (const BDD &notMutexBDD: notMutexBDDs) {
                        //cout << res.nodeCount()  << " and " <<  notMutexBDD.nodeCount()  << flush;
                        res = res.And(notMutexBDD, nodeLimit);
                        //cout << ": " << res.nodeCount() << endl;
                    }
                }
                break;
            case MutexType::MUTEX_AND:
                for (const BDD &notMutexBDD: notMutexBDDs) {
                    DEBUG_MSG(std::cout << "Filter: " << res.nodeCount() << " and " << notMutexBDD.nodeCount() << std::flush;);
                    res = res.And(notMutexBDD, nodeLimit);
                    DEBUG_MSG(std::cout << ": " << res.nodeCount() << std::endl;);
                }
                break;
            case MutexType::MUTEX_RESTRICT:
                for (const BDD &notMutexBDD: notMutexBDDs)
                    res = res.Restrict(notMutexBDD);
                break;
            case MutexType::MUTEX_NPAND:
                for (const BDD &notMutexBDD: notMutexBDDs)
                    res = res.NPAnd(notMutexBDD);
                break;
            case MutexType::MUTEX_CONSTRAIN:
                for (const BDD &notMutexBDD: notMutexBDDs)
                    res = res.Constrain(notMutexBDD);
                break;
            case MutexType::MUTEX_LICOMP:
                for (const BDD &notMutexBDD: notMutexBDDs)
                    res = res.LICompaction(notMutexBDD);
                break;
        }
        return res;
    }

    int SymManager::filterMutexBucket(std::vector <BDD> &bucket, bool fw,
                                      bool initialization, int maxTime, int maxNodes) {
        int numFiltered = 0;
        setTimeLimit(maxTime);
        try {
            for (auto & item : bucket) {
                DEBUG_MSG(std::cout << "Filter spurious " << (fw ? "fw" : "bw") << ": ";
                                  if (abstraction) {
                                      std::cout << *abstraction;
                                  } else {
                                      std::cout << "original state space";
                                  }
                                  std::cout << " from: " << item.nodeCount() <<
                                            " maxTime: " << maxTime << " and maxNodes: " << maxNodes;);

                item = filter_mutex(item, fw, maxNodes, initialization);
                DEBUG_MSG(std::cout << " => " << item.nodeCount() << std::endl;);
                numFiltered++;
            }
        } catch (BDDError e) {
            DEBUG_MSG(std::cout << " truncated." << std::endl;);
        }
        unsetTimeLimit();

        return numFiltered;
    }
     */

BDD SymManager::simulatedBy(const BDD &bdd, bool fw) {
    if (prune_heuristic && fw && prune_heuristic->use_dominance_pruning()) {
        setTimeLimit(p.max_mutex_time);
        try {
            BDD res = prune_heuristic->simulatedBy(bdd);
            unsetTimeLimit();
            // TODO mutexes not supported yet
            // res = filter_mutex(res, true, 1000000, true);
            // res = filter_mutex(res, false, 1000000, true);
            return res;
        } catch (BDDError &e) {
            unsetTimeLimit();
            exit(0);
        }
    } else {
        return bdd;
    }
}

void SymManager::init_simulation() {
    if (prune_heuristic) {
        prune_heuristic->initialize(this);
    }
}
}
