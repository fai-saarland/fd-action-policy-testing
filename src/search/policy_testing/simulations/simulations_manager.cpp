#include "simulations_manager.h"

namespace simulations {
    void PrePost::dump() const {
        std::cout << global_simulation_task()->get_variable_name(var) << ": " << pre << " => " << post;
        if (!cond.empty()) {
            std::cout << " if";
            for (int i = 0; i < cond.size(); i++) {
                std::cout << " ";
                cond[i].dump();
            }
        }
    }

    void Prevail::dump() const {
        std::cout << global_simulation_task()->get_variable_name(var) << ": " << prev;
    }
} // simulations