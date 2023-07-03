#include "sym_test_mutex.h"

#include "sym_manager.h"

/*

namespace simulations {

    void GSTMutex::check_mutexes(SymManager &manager) {
        SymAbstraction *abs = manager.getAbstraction();
        BDD goal = manager.getGoal();
        if (!abs->isAbstracted()) {
            notMutexBDD = manager.oneBDD();
            for (const auto &bdd: manager.getNotMutexBDDs(false)) {
                notMutexBDD *= bdd;
            }

            std::cout << "#MUTEX BDD: " << notMutexBDD.nodeCount() << std::endl;
            BDD goalwoMutex = goal * notMutexBDD;
            std::cout << "#MUTEX GOAL: " << goal.nodeCount() << " => " <<
                 goalwoMutex.nodeCount() << std::endl;
        } else {
            BDD abstractNotMutexBDD = manager.oneBDD();
            for (auto &bdd: manager.getNotMutexBDDs(false)) {
                abstractNotMutexBDD *= bdd;
            }

            BDD abstractedBDD = abs->shrinkExists(notMutexBDD, 10000000);

            std::cout << "#MUTEX ABSTRACT " << *abs << " BDD: " <<
                 (abstractedBDD == abstractNotMutexBDD ? " is equal " : " is different") <<
                 " abstract bdd: " << abstractNotMutexBDD.nodeCount() <<
                 " shrinked original: " << abstractedBDD.nodeCount() << std::endl;

            BDD goalAbstract = goal * abstractNotMutexBDD;
            BDD goalAbstractShrinked = goal * abstractedBDD;

            std::cout << "#MUTEX ABSTRACT GOAL: " << goalAbstract.nodeCount() <<
                 " shrinked mutex goal: " << goalAbstractShrinked.nodeCount() << std::endl;


        }
    }

    GSTMutex gst_mutex;

}
 */
