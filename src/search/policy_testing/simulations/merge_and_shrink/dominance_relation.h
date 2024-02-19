#pragma once

#include <vector>
#include <memory>
#include "abstraction.h"
#include "labels.h"
#include "simulation_relation.h"
#include "../../../utils/timer.h"

namespace simulations {
class LabelledTransitionSystem;

/*
 * Class that represents the collection of simulation relations for a
 * factored LTS. Uses unique_ptr so that it owns the simulations, and
 * it cannot be copied away.
 */
class DominanceRelation {
protected:
    std::vector<std::unique_ptr<SimulationRelation>> simulations;

    virtual std::unique_ptr<SimulationRelation> init_simulation(Abstraction *_abs) = 0;

    virtual std::unique_ptr<SimulationRelation>
    init_simulation_incremental(CompositeAbstraction *_abs,
                                const SimulationRelation &simrel_one,
                                const SimulationRelation &simrel_two) = 0;

public:
    virtual ~DominanceRelation() = default;

    //Methods to use the simulation
    // [[nodiscard]] bool pruned_state(const State &state) const;

    [[nodiscard]] int get_cost(const State &state) const;

    [[nodiscard]] bool dominates(const State &t, const State &s) const;


    void init(const std::vector<Abstraction *> &abstractions);

    void init_incremental(CompositeAbstraction *_abs,
                          const SimulationRelation &simrel_one,
                          const SimulationRelation &simrel_two);

    virtual void compute_ld_simulation(std::vector<LabelledTransitionSystem *> &_ltss,
                                       const LabelMap &labelMap,
                                       bool incremental_step, bool dump) = 0;


    [[nodiscard]] virtual bool propagate_transition_pruning(int lts_id,
                                                            const std::vector<LabelledTransitionSystem *> &ltss,
                                                            int src, int label_id, int target) const = 0;

    virtual int prune_subsumed_transitions(std::vector<Abstraction *> &abstractions,
                                           const LabelMap &labelMap,
                                           const std::vector<LabelledTransitionSystem *> &ltss,
                                           int lts_id, bool preserve_all_optimal_plans) = 0;


    virtual EquivalenceRelation *get_equivalent_labels_relation(const LabelMap &labelMap,
                                                                std::set<int> &dangerous_LTSs) const = 0;

    void remove_useless();


    //Statistics of the factored simulation
    void dump_statistics(bool expensive_statistics) const;

    [[nodiscard]] int num_equivalences() const;

    [[nodiscard]] int num_simulations() const;

    [[nodiscard]] double num_st_pairs() const;

    [[nodiscard]] double num_states_problem() const;

    //Computes the probability of selecting a random pair s, s' such that
    //s is equivalent to s'.
    //[[nodiscard]] double get_percentage_equivalences() const;
    //[[nodiscard]] double get_percentage_equal() const;

    //Methods to access the underlying simulation relations
    [[nodiscard]] const std::vector<std::unique_ptr<SimulationRelation>> &get_simulations() const {
        return simulations;
    }

    [[nodiscard]] int size() const {
        return simulations.size();
    }

    SimulationRelation &operator[](int index) {
        return *(simulations[index]);
    }

    const SimulationRelation &operator[](int index) const {
        return *(simulations[index]);
    }

    void clear() {
        std::vector<std::unique_ptr<SimulationRelation>>().swap(simulations);
    }
};

/*
 * Class that represents the collection of simulation relations for a
 * factored LTS. Uses unique_ptr so that it owns the simulations, and
 * it cannot be copied away.
 */
template<typename LR>
class DominanceRelationLR : public DominanceRelation {
    LR label_dominance;


    virtual void update(int lts_id, const LabelledTransitionSystem *lts,
                        const LR &label_dominance,
                        SimulationRelation &simrel) = 0;

    /*
    bool propagate_label_domination(int lts_id,
                                    const LabelledTransitionSystem *lts,
                                    int l, int l2, SimulationRelation &simrel) const;
    */

    template<typename LTS>
    void compute_ld_simulation_template(std::vector<LTS *> &_ltss,
                                        const LabelMap &labelMap,
                                        bool incremental_step,
                                        bool dump) {
        assert(_ltss.size() == simulations.size());
        utils::Timer t;

        int total_size = 0, max_size = 0, total_trsize = 0, max_trsize = 0;
        for (auto lts: _ltss) {
            max_size = std::max(max_size, lts->size());
            max_trsize = std::max(max_trsize, lts->num_transitions());
            total_size += lts->size();
            total_trsize += lts->num_transitions();
        }
        std::cout << "Compute LDSim on " << _ltss.size() << " LTSs."
                  << " Total size: " << total_size
                  << " Total trsize: " << total_trsize
                  << " Max size: " << max_size
                  << " Max trsize: " << max_trsize
                  << std::endl;

        label_dominance.init(_ltss, *this, labelMap);

        std::cout << "Init LDSim in " << t() << ":" << std::endl;
        do {
            //label_dominance.dump();
            if (incremental_step) {
                // Should be enough to just update the last (i.e., the new) simulation here.
                update(simulations.size() - 1, _ltss.back(),
                       label_dominance, *(simulations.back()));
            } else {
                for (int i = 0; i < simulations.size(); i++) {
                    update(i, _ltss[i], label_dominance, *(simulations[i]));
                    //_dominance_relation[i]->dump(_ltss[i]->get_names());
                }
            }
            std::cout << " " << t() << std::endl;
            //return; //PIET-edit: remove this for actual runs; just here for debugging the complex stuff
        } while (label_dominance.update(_ltss, *this));
        std::cout << std::endl << "LDSim computed " << t() << std::endl;
        if (dump) {
            for (int i = 0; i < _ltss.size(); i++) {
                //_ltss[i]->dump();
                simulations[i]->dump(_ltss[i]->get_names());
            }
        }
    }

public:
    explicit DominanceRelationLR(Labels *labels) : label_dominance(labels) {}


    void compute_ld_simulation(std::vector<LabelledTransitionSystem *> &_ltss,
                               const LabelMap &labelMap,
                               bool incremental_step, bool dump) override {
        compute_ld_simulation_template(_ltss, labelMap, incremental_step, dump);
    }

    [[nodiscard]] bool propagate_transition_pruning(int lts_id,
                                                    const std::vector<LabelledTransitionSystem *> &ltss,
                                                    int src, int label_id, int target) const override {
        return label_dominance.propagate_transition_pruning(lts_id, ltss, *this, src, label_id, target);
    }


// If lts_id = -1 (default), then prunes in all ltss. If lts_id > 0,
// prunes transitions dominated in all LTS, but other
// transitions are only checked for lts_id
    int prune_subsumed_transitions(std::vector<Abstraction *> &abstractions,
                                   const LabelMap &labelMap,
                                   const std::vector<LabelledTransitionSystem *> &ltss,
                                   int lts_id, bool preserve_all_optimal_plans) override {
        int num_pruned_transitions = 0;

        //a) prune transitions of labels that are completely dominated by
        //other in all LTS
        if (!preserve_all_optimal_plans) {
            std::vector<int> labels_id = label_dominance.get_labels_dominated_in_all();
            for (auto abs: abstractions) {
                for (int l: labels_id) {
                    num_pruned_transitions += abs->prune_transitions_dominated_label_all(labelMap.get_old_id(l));
                    label_dominance.kill_label(l);
                }
            }
        }

        //b) prune transitions dominated by noop in a transition system
        for (int l = 0; l < label_dominance.get_num_labels(); l++) {
            int lts = label_dominance.get_dominated_by_noop_in(l);
            if (lts >= 0 && (lts == lts_id || lts_id == -1)) {
                // the index of the LTS and its corresponding abstraction should always be the same -- be careful about
                // this in the other code!

                num_pruned_transitions += abstractions[lts]->
                    prune_transitions_dominated_label_noop(lts, ltss,
                                                           *this,
                                                           labelMap, labelMap.get_old_id(l));
            }
        }

        if (!preserve_all_optimal_plans) {
            //c) prune transitions dominated by other transitions
            for (int lts = 0; lts < abstractions.size(); lts++) {
                if (lts_id != -1 && lts != lts_id)
                    continue;
                Abstraction *abs = abstractions[lts];
                const auto &is_rel_label = abs->get_relevant_labels();
                //l : Iterate over relevant labels
                for (int l = 0; l < is_rel_label.size(); ++l) {
                    if (!is_rel_label[l])
                        continue;
                    int label_l = labelMap.get_id(l);
                    for (int l2 = l; l2 < is_rel_label.size(); ++l2) {
                        if (!is_rel_label[l2])
                            continue;
                        int label_l2 = labelMap.get_id(l2);
                        //l2 : Iterate over relevant labels
                        /* PIET-edit: after talking to Alvaro, it seems that the only critical case, i.e., having two labels l and l'
                         * with l >= l' and l' >= l, and two transitions, s--l->t and s--l'->t' with t >= t' and t' >= t, cannot occur
                         * if we first run simulation-shrinking. So, if we make sure that it is run before irrelevance pruning, we
                         * should have no problems here.
                         */
                        if (label_dominance.dominates(label_l2, label_l, lts)
                            && label_dominance.dominates(label_l, label_l2, lts)) {
                            num_pruned_transitions +=
                                abs->prune_transitions_dominated_label_equiv(lts, ltss, *this, labelMap, l, l2);
                        } else if (label_dominance.dominates(label_l2, label_l, lts)) {
                            num_pruned_transitions +=
                                abs->prune_transitions_dominated_label(lts, ltss, *this, labelMap, l, l2);
                        } else if (label_dominance.dominates(label_l, label_l2, lts)) {
                            num_pruned_transitions +=
                                abs->prune_transitions_dominated_label(lts, ltss, *this, labelMap, l2, l);
                        }
                    }
                }
            }
        }
        return num_pruned_transitions;
    }

    EquivalenceRelation *get_equivalent_labels_relation(const LabelMap &labelMap,
                                                        std::set<int> &dangerous_LTSs) const override {
        return label_dominance.get_equivalent_labels_relation(labelMap, dangerous_LTSs);
    }
};
}
