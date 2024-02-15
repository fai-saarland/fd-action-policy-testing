#include "merge_strategy.h"

#include "../simulations_manager.h"
#include "../../../plugins/plugin.h"

#include <iostream>

namespace simulations {
MergeStrategy::MergeStrategy() {
    add_init_function([&]() {
                          assert(total_merges == -1);
                          assert(remaining_merges == -1);
                          total_merges = global_simulation_task()->get_num_variables() - 1;
                          remaining_merges = global_simulation_task()->get_num_variables() - 1;
                          // There are number of variables many atomic abstractions and we have
                          // to perform one less merges than this number until we have merged
                          // all abstractions into one composite abstraction.
                      });
}

void MergeStrategy::dump_options() const {
    std::cout << "Merge strategy: " << name() << std::endl;
    dump_strategy_specific_options();
}

static class MergeStrategyPlugin : public plugins::TypedCategoryPlugin<MergeStrategy> {
public:
    MergeStrategyPlugin() : TypedCategoryPlugin("MergeStrategy") {
        document_synopsis(
            "This page describes the different merge strategies."
            );
    }
}
_category_plugin;
}
