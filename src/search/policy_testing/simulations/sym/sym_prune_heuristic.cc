#include "sym_prune_heuristic.h"

#include <memory>

#include "../merge_and_shrink/ld_simulation.h"
#include "sym_transition.h"

#include "../../../option_parser.h"
#include "../../../plugin.h"
#include "../../../utils/timer.h"
#include "sym_manager.h"

namespace simulations {
SymPruneHeuristic::SymPruneHeuristic(const options::Options &opts) :
    cost_type(opts.get<OperatorCost>("cost_type")),
    prune_irrelevant(opts.get<bool>("prune_irrelevant")),
    dominance_pruning(opts.get<bool>("dominance_pruning")),
    abstractionBuilder(opts.get<AbstractionBuilder *>("abs")) {
    // TODO OperatorCost::ZERO not supported
    abstractionBuilder->build_abstraction(/*cost_type == OperatorCost::ZERO || */ is_unit_cost_task(cost_type),
                                          cost_type,
                                          ldSimulation, abstractions);
}

void SymPruneHeuristic::initialize(SymManager *mgr) {
    std::cout << "Initialize sym prune heuristic" << std::endl;
    if (!tr && dominance_pruning) {
        tr = std::make_unique<SymTransition>(mgr, ldSimulation->get_dominance_relation());
    }

    if (prune_irrelevant) {
        std::cout << "Computing irrelevant states BDD " << utils::g_timer() << std::endl;
        BDD irrelevantStates = ldSimulation->get_dominance_relation().getIrrelevantStates(mgr->getVars());
        std::cout << "Irrelevant states BDD: " << irrelevantStates.nodeCount() << " " << utils::g_timer() << std::endl;
        //Prune irrelevant states in both directions
        mgr->addDeadEndStates(true, irrelevantStates);
        mgr->addDeadEndStates(false, irrelevantStates);
    }
}

BDD SymPruneHeuristic::simulatedBy(const BDD &bdd) {
    if (dominance_pruning)
        return tr->image(bdd);
    else
        return bdd;
}

static std::shared_ptr<SymPruneHeuristic> _parse(options::OptionParser &parser) {
    parser.document_synopsis("Simulation prune heuristic", "");

    parser.add_option<bool>("prune_irrelevant",
                            "Activate removing irrelevant states from the search",
                            "false");

    parser.add_option<bool>("dominance_pruning", "Activate dominance pruning", "false");
    parser.add_option<std::shared_ptr<AbstractionBuilder>>("abs", "abstraction builder", "");


    options::Options opts = parser.parse();

    if (parser.dry_run()) {
        return nullptr;
    } else {
        return std::make_shared<SymPruneHeuristic>(opts);
    }
}

static PluginTypePlugin<SymPruneHeuristic> _plugin_type_simulation("simulation", "");
static Plugin<SymPruneHeuristic> _plugin("simulation", _parse);
}
