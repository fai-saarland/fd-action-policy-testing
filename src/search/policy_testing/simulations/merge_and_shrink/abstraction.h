#pragma once

#include "shrink_strategy.h"
#include "../utils/utilities.h"
#include "../../../task_proxy.h"
#include "../simulations_manager.h"

#include <boost/dynamic_bitset.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <utility>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/unique_ptr.hpp>
#include <boost/serialization/export.hpp>
#include <string>
#include <vector>

namespace simulations {
inline static constexpr const int PLUS_INFINITY = std::numeric_limits<int>::max();
inline static constexpr const int MINUS_INFINITY = std::numeric_limits<int>::min();

class EquivalenceRelation;
class Label;
class Labels;
class LabelMap;
class SimulationRelation;
class LabelledTransitionSystem;
class DominanceRelation;
class IntEpsilonSum;

typedef int AbstractStateRef;

struct AbstractTransition {
    AbstractStateRef src;
    AbstractStateRef target;

    AbstractTransition(AbstractStateRef src_, AbstractStateRef target_)
        : src(src_), target(target_) {
    }

    bool operator==(const AbstractTransition &other) const {
        return src == other.src && target == other.target;
    }

    bool operator!=(const AbstractTransition &other) const {
        return !(*this == other);
    }

    bool operator<(const AbstractTransition &other) const {
        return src < other.src || (src == other.src && target < other.target);
    }

    bool operator>=(const AbstractTransition &other) const {
        return !(*this < other);
    }
};

struct StrippedAbstraction;

class Abstraction {
    friend class AtomicAbstraction;
    friend class CompositeAbstraction;
    friend class PDBAbstraction;

    friend class StrippedCompositeAbstraction;
    friend class ShrinkStrategy;     // for apply() -- TODO: refactor!
    friend class SimulationRelation;     // for apply() -- TODO: refactor!
    friend class LDSimulation;     // for setting store_original_operators -- TODO: refactor!
    friend class AbsBuilderMasSimulation;     // for setting store_original_operators -- TODO: refactor!
    friend class AbsBuilderMAS;     // for setting store_original_operators -- TODO: refactor!
    friend class AbsBuilderDefault;     // for setting store_original_operators -- TODO: refactor!
    friend class AbsBuilderAtomicAdvanced;

    inline static constexpr const int PRUNED_STATE = -1;
    inline static constexpr const int DISTANCE_UNKNOWN = -2;

    static bool store_original_operators;

    // There should only be one instance of Labels at runtime. It is created
    // and managed by MergeAndShrinkHeuristic. All abstraction instances have
    // a copy of this object to ease access to the set of labels.
    // Alvaro: Removed const to allow setting the abstraction as
    // irrelevant for some labels during normalize().
    Labels *labels;
    /* num_labels equals to the number of labels that this abstraction is
       "aware of", i.e. that have
       been incorporated into transitions_by_label. Whenever new labels are
       generated through label reduction, we do *not* update all abstractions
       immediately. This equals labels->size() after normalizing. */
    int num_labels;
    /* transitions_by_label and relevant_labels both have size of (2 * n) - 1
       if n is the number of operators, because when applying label reduction,
       at most n - 1 fresh labels can be generated in addition to the n
       original labels. */
    std::vector<std::vector<AbstractTransition>> transitions_by_label;
    std::vector<std::vector<boost::dynamic_bitset<>>> transitions_by_label_based_on_operators;

    std::vector<bool> relevant_labels;

    //TODO: Unify with transitions by label??
    std::unique_ptr<LabelledTransitionSystem> lts;
    /* std::unique_ptr<LTSComplex> lts_complex; */

    // Alvaro: Information regarding the number of transitions by label
    // (needed by some merge_criteria, only computed on demand)
    // TODO: added as attribute in abstractions to avoid recomputation
    // when different criteria use this data. Move somewhere else?
    std::vector<int> num_transitions_by_label;
    std::vector<int> num_goal_transitions_by_label;

    int num_states;

    std::vector<int> init_distances;
    std::vector<int> goal_distances;
    std::vector<bool> goal_states;
    AbstractStateRef init_state;

    int max_f;
    int max_g;
    int max_h;

    bool transitions_sorted_unique;
    //Alvaro: substituted goal_relevant by number of goal variables =>
    //allow easy check of whether all the goal variables are relevant
    int goal_relevant_vars;
    bool all_goals_relevant;

    mutable unsigned int peak_memory;

    // Need this pointer to the simulation relation in order to update its table in case of shrinking
    SimulationRelation *simulation_relation;

    void clear_distances();
    void compute_init_distances_unit_cost();
    void compute_goal_distances_unit_cost();
    void compute_init_distances_general_cost();
    void compute_goal_distances_general_cost();

    //Alvaro: Computes num_transitions_by_label and
    //num_goal_transitions_by_label
    void count_transitions_by_label();

    // are_transitions_sorted_unique() is used to determine whether the
    // transitions of an abstraction are sorted uniquely or not after
    // construction (composite abstraction) and shrinking (apply_abstraction).
    bool are_transitions_sorted_unique() const;

    void apply_abstraction(std::vector<__gnu_cxx::slist<AbstractStateRef>> &collapsed_groups);

    int unique_unlabeled_transitions() const;

protected:
    std::vector<int> varset;

    virtual void apply_abstraction_to_lookup_table(const std::vector<AbstractStateRef> &abstraction_mapping) = 0;
    virtual unsigned int memory_estimate() const;

public:
    explicit Abstraction(Labels *labels);

    Abstraction(const Abstraction &o);

    virtual ~Abstraction() = default;

    int total_transitions() const;
    int total_transition_operators() const;

    //bool assert_no_dead_labels() const ;

    // Two methods to identify the abstraction in output.
    // tag is a convenience method that upper-cases the first letter of
    // description and appends ": ";
    virtual std::string description() const = 0;

    std::string tag() const;

    //Returns a description of the abstract state
    virtual std::string description(int s) const {
        return std::to_string(s);
    }

    static void build_atomic_abstractions(std::vector<Abstraction *> &result,
                                          Labels *labels);

    bool is_solvable() const;

    int get_cost(const State &state) const;
    int size() const;
    void statistics(bool include_expensive_statistics) const;
    const std::string &label_name(int l) const;

    int get_peak_memory_estimate() const;
    // NOTE: This will only return something useful if the memory estimates
    //       have been computed along the way by calls to statistics().
    // TODO: Find a better way of doing this that doesn't require
    //       a mutable attribute?

    bool are_distances_computed() const;
    void compute_distances();
    bool is_normalized() const;
    void normalize();
    void normalize2(); // Version of normalize handling the storing of original operators in the transitions
    EquivalenceRelation *compute_local_equivalence_relation() const;
    void release_memory();

    void dump_relevant_labels() const;
    void dump() const;
    void dump_names() const;

    // The following methods exist for the benefit of shrink strategies.
    int get_max_f() const;
    int get_max_g() const; // Not being used!
    int get_max_h() const;

    bool is_goal_state(int state) const {
        return goal_states[state];
    }

    int get_init_distance(int state) const {
        return init_distances[state];
    }

    bool is_useless() const {
        return num_states == 1;
    }

    const std::vector<int> &get_goal_distances() const {
        return goal_distances;
    }

    std::vector<IntEpsilonSum> recompute_goal_distances_with_epsilon() const;

    int get_goal_distance(int state) const {
        return goal_distances[state];
    }

    // These methods should be private but is public for shrink_bisimulation
    int get_label_cost_by_index(int label_no) const;
    const std::vector<AbstractTransition> &get_transitions_for_label(int label_no) const;
    const std::vector<boost::dynamic_bitset<>> &get_transition_ops_for_label(int label_no) const;
    // This method is shrink_bisimulation-exclusive
    int get_num_labels() const;
    int get_num_nonreduced_labels() const;
    // These methods are used by non_linear_merge_strategy
    void compute_label_ranks(std::vector<int> &label_ranks);
    bool is_goal_relevant() const {
        return goal_relevant_vars > 0;
    }
    bool get_all_goal_vars_in() const {
        return all_goals_relevant;
    }
    // This is used by the "old label reduction" method
    const std::vector<int> &get_varset() const {
        return varset;
    }

    bool is_atomic() const {
        return varset.size() == 1;
    }

    const std::vector<bool> &get_relevant_labels() const {
        return relevant_labels;
    }

    virtual AbstractStateRef get_abstract_state(const State &state) const = 0;
    virtual AbstractStateRef get_abstract_state(const std::vector<int> &state) const = 0;

    /**
    * Returns the abstract state for a local state value in an atomic abstraction.
    * @warning only usable for atomic abstractions.
    */
    virtual AbstractStateRef get_atomic_abstract_state(int) const {
        assert(false);
        return {};
    }

    LabelledTransitionSystem *get_lts(const LabelMap &labelMap);
    /* LTSComplex * get_lts_complex(const LabelMap & labelMap); */

    //Alvaro: used by shrink_empty_labels
    const std::vector<bool> &get_goal_states() const {
        return goal_states;
    }

    //Alvaro: used by merge criteria. For each remaining
    //abstraction, counts the number of transitions in the
    //transitions labelled with a relevant label for that
    //abstraction. If only_empty is activated, only the transitions
    //that are relevant for 2 abstractions are counted (this
    //abstraction and the other). If only_goal is activated, only
    //transitions leading to a goal state are counted.
    void count_transitions(const std::vector<Abstraction *> &all_abstractions,
                           const std::vector<int> &remaining, bool only_empty,
                           bool only_goal, std::vector<int> &result);

    bool is_own_label(int label_no);

    void reset_lts();

    // Methods to prune irrelevant transitions Prune all the
    // transitions of a given label (it is completely dominated by
    // some other label or by noop)
    unsigned int prune_transitions_dominated_label_all(int label_no);

    // Prune all the transitions of label_no such that exist a better
    // transition for label_no_by
    int prune_transitions_dominated_label(int lts_id,
                                          const std::vector<LabelledTransitionSystem *> &ltss,
                                          const DominanceRelation &simulations, const LabelMap &labelMap,
                                          int label_no, int label_no_by);

    // Prune all the transitions of label_no such that exist a better
    // transition for label_no_by
    int prune_transitions_dominated_label_equiv(int lts_id,
                                                const std::vector<LabelledTransitionSystem *> &ltss,
                                                const DominanceRelation &simulations,
                                                const LabelMap &labelMap,
                                                int label_no, int label_no_by);

    //Prune all the transitions dominated by noop
    int prune_transitions_dominated_label_noop(int lts_id,
                                               const std::vector<LabelledTransitionSystem *> &ltss,
                                               const DominanceRelation &simulations, const LabelMap &labelMap,
                                               int label_no);

    int estimate_transitions(const Abstraction *other) const;

    bool is_dead_end(const State &state) const {
        return get_abstract_state(state) == -1;
    }

    void get_dead_labels(std::vector<bool> &dead_labels,
                         std::vector<int> &new_dead_labels);

    bool check_dead_operators(std::vector<bool> &dead_labels, std::vector<bool> &dead_operators) const;

    void set_simulation_relation(SimulationRelation *simrel) {
        simulation_relation = simrel;
    }

    const SimulationRelation &get_simulation_relation() const {
        return *simulation_relation;
    }

    virtual Abstraction *clone() const = 0;

    virtual std::unique_ptr<StrippedAbstraction> strip() const = 0;
};

struct StrippedAbstraction {
    [[nodiscard]] virtual AbstractStateRef get_abstract_state(const State &state) const = 0;

    [[nodiscard]] virtual AbstractStateRef get_abstract_state(const std::vector<int> &state) const = 0;

    [[nodiscard]] virtual AbstractStateRef get_atomic_abstract_state(int) const {
        assert(false);
        return {};
    }

    virtual bool operator==(const StrippedAbstraction &other) const = 0;

    virtual ~StrippedAbstraction() = default;
    template<typename Archive>
    void serialize(Archive &, const unsigned int /*version*/) {
    }
};

BOOST_SERIALIZATION_ASSUME_ABSTRACT(StrippedAbstraction)

class StrippedAtomicAbstraction : public StrippedAbstraction {
    int variable = -1;
    std::vector<AbstractStateRef> lookup_table;

public:

    StrippedAtomicAbstraction() = default;

    StrippedAtomicAbstraction(int variable, std::vector<AbstractStateRef> lookup_table) :
        variable(variable), lookup_table(std::move(lookup_table)) {}

    ~StrippedAtomicAbstraction() override = default;

    [[nodiscard]] int get_variable() const {return variable;}

    [[nodiscard]] AbstractStateRef get_abstract_state(const State &state) const override {
        return lookup_table[state[variable].get_value()];
    }

    [[nodiscard]] AbstractStateRef get_abstract_state(const std::vector<int> &state) const override {
        return lookup_table[state[variable]];
    }

    [[nodiscard]] AbstractStateRef get_atomic_abstract_state(int local_state_value) const override {
        return lookup_table[local_state_value];
    }

    bool operator==(const StrippedAbstraction &other) const override {
        const auto *other_ptr = dynamic_cast<const StrippedAtomicAbstraction *>(&other);
        return other_ptr && other_ptr->variable == variable && other_ptr->lookup_table == lookup_table;
    }

    friend class boost::serialization::access;
    template<typename Archive>
    void serialize(Archive &ar, const unsigned int /*version*/) {
        ar &boost::serialization::base_object<StrippedAbstraction>(*this);
        ar &variable;
        ar &lookup_table;
    }
};


class AtomicAbstraction : public Abstraction {
    friend class StrippedAtomicAbstraction;
    int variable;
    std::vector<AbstractStateRef> lookup_table;
protected:
    std::string description() const override;
    std::string description(int s) const override;
    void apply_abstraction_to_lookup_table(const std::vector<AbstractStateRef> &abstraction_mapping) override;
    unsigned int memory_estimate() const override;
public:
    AtomicAbstraction(const AtomicAbstraction &) = default;
    AtomicAbstraction(Labels *labels, int variable);
    ~AtomicAbstraction() override = default;

    int get_variable() const {return variable;}
    AbstractStateRef get_abstract_state(const State &state) const override;
    AbstractStateRef get_abstract_state(const std::vector<int> &state) const override;
    AbstractStateRef get_atomic_abstract_state(int local_state_value) const override;

    Abstraction *clone() const override;
    std::unique_ptr<StrippedAbstraction> strip() const override {
        return std::make_unique<StrippedAtomicAbstraction>(variable, lookup_table);
    }
};

class StrippedCompositeAbstraction : public StrippedAbstraction {
    std::unique_ptr<StrippedAbstraction> component_0;
    std::unique_ptr<StrippedAbstraction> component_1;
    std::vector<std::vector<AbstractStateRef>> lookup_table;

public:
    StrippedCompositeAbstraction() = default;
    StrippedCompositeAbstraction(std::unique_ptr<StrippedAbstraction> &&component_0,
                                 std::unique_ptr<StrippedAbstraction> &&component_1,
                                 std::vector<std::vector<AbstractStateRef>> lookup_table) :
        component_0(std::move(component_0)), component_1(std::move(component_1)),
        lookup_table(std::move(lookup_table)) {
    }

    ~StrippedCompositeAbstraction() override = default;

    [[nodiscard]] AbstractStateRef get_abstract_state(const State &state) const override {
        AbstractStateRef state1 = component_0->get_abstract_state(state);
        AbstractStateRef state2 = component_1->get_abstract_state(state);
        if (state1 == Abstraction::PRUNED_STATE || state2 == Abstraction::PRUNED_STATE)
            return Abstraction::PRUNED_STATE;
        return lookup_table[state1][state2];
    }

    [[nodiscard]] AbstractStateRef get_abstract_state(const std::vector<int> &state) const override {
        AbstractStateRef state1 = component_0->get_abstract_state(state);
        AbstractStateRef state2 = component_1->get_abstract_state(state);
        if (state1 == Abstraction::PRUNED_STATE || state2 == Abstraction::PRUNED_STATE)
            return Abstraction::PRUNED_STATE;
        return lookup_table[state1][state2];
    }

    bool operator==(const StrippedAbstraction &other) const override {
        const auto *other_ptr = dynamic_cast<const StrippedCompositeAbstraction *>(&other);
        if (!other_ptr) {
            return false;
        }
        assert(component_0 && component_1 && other_ptr->component_0 && other_ptr->component_1);
        return *other_ptr->component_0 == *component_0 && *other_ptr->component_1 == *component_1 &&
               other_ptr->lookup_table == lookup_table;
    }

    friend class boost::serialization::access;
    template<typename Archive>
    void serialize(Archive &ar, const unsigned int /*version*/) {
        ar &boost::serialization::base_object<StrippedAbstraction>(*this);
        ar &component_0;
        ar &component_1;
        ar &lookup_table;
    }
};

class CompositeAbstraction : public Abstraction {
    friend class StrippedCompositeAbstraction;
    Abstraction *components[2];
    std::vector<std::vector<AbstractStateRef>> lookup_table;
protected:
    std::string description() const override;
    std::string description(int s) const override;
    void apply_abstraction_to_lookup_table(
        const std::vector<AbstractStateRef> &abstraction_mapping) override;
    unsigned int memory_estimate() const override;
public:
    CompositeAbstraction(const CompositeAbstraction &);
    CompositeAbstraction(Labels *labels, Abstraction *abs1, Abstraction *abs2);
    ~CompositeAbstraction() override = default;

    AbstractStateRef get_abstract_state(const State &state) const override;
    AbstractStateRef get_abstract_state(const std::vector<int> &state) const override;

    const Abstraction *get_component(const int id) const {
        assert(id == 0 || id == 1);
        return components[id];
    }
    int get_abstract_state(int i, int j) const {
        return lookup_table[i][j];
    }
    Abstraction *clone() const override;
    std::unique_ptr<StrippedAbstraction> strip() const override {
        return std::make_unique<StrippedCompositeAbstraction>(components[0]->strip(),
                                                              components[1]->strip(), lookup_table);
    }
};

class StrippedPDBAbstraction : public StrippedAbstraction {
    std::vector<int> pattern;
    std::vector<AbstractStateRef> lookup_table;

public:
    StrippedPDBAbstraction() = default;
    explicit StrippedPDBAbstraction(std::vector<int>  pattern, std::vector<AbstractStateRef> lookup_table) :
        pattern(std::move(pattern)), lookup_table(std::move(lookup_table)) {
    }

    ~StrippedPDBAbstraction() override = default;

    template<typename T>
    [[nodiscard]] int rank(const T &state) const {
        int res = 0;
        for (int i: pattern) {
            int v = i;
            if (res)
                res *= global_simulation_task()->get_variable_domain_size(i);
            if constexpr (std::is_same_v<T, State>) {
                res += state[v].get_value();
            } else {
                res += state[v];
            }
        }
        return res;
    }

    [[nodiscard]] AbstractStateRef get_abstract_state(const State &state) const override {
        return lookup_table[rank(state)];
    }

    [[nodiscard]] AbstractStateRef get_abstract_state(const std::vector<int> &state) const override {
        return lookup_table[rank(state)];
    }

    bool operator==(const StrippedAbstraction &other) const override {
        const auto *other_ptr = dynamic_cast<const StrippedPDBAbstraction *>(&other);
        return other_ptr && other_ptr->pattern == pattern && other_ptr->lookup_table == lookup_table;
    }

    friend class boost::serialization::access;
    template<typename Archive>
    void serialize(Archive &ar, const unsigned int /*version*/) {
        ar &boost::serialization::base_object<StrippedAbstraction>(*this);
        ar &pattern;
        ar &lookup_table;
    }
};

class PDBAbstraction : public Abstraction {
    friend class StrippedPDBAbstraction;
    // List of variables of the abstraction (ordered to perform ranking)
    std::vector<int> pattern;
    std::vector<AbstractStateRef> lookup_table;
private:

    PDBAbstraction(const PDBAbstraction &) = default;

    template<typename T>
    int rank(const T &state) const {
        int res = 0;
        for (int i: pattern) {
            int v = i;
            if (res)
                res *= global_simulation_task()->get_variable_domain_size(i);
            if constexpr (std::is_same_v<T, State>) {
                res += state[v].get_value();
            } else {
                res += state[v];
            }
        }
        return res;
    }


    void insert_transitions(std::vector<int> &pre_vals, std::vector<int> &eff_vals,
                            int label_no, int pos);

    void insert_goals(std::vector<int> &goal_vals, int pos);


protected:
    std::string description() const override;
    std::string description(int s) const override;
    void apply_abstraction_to_lookup_table(
        const std::vector<AbstractStateRef> &abstraction_mapping) override;
    unsigned int memory_estimate() const override;

public:
    PDBAbstraction(Labels *labels, std::vector<int> pattern_);
    ~PDBAbstraction() override = default;

    AbstractStateRef get_abstract_state(const State &state) const override;
    AbstractStateRef get_abstract_state(const std::vector<int> &state) const override;

    Abstraction *clone() const override;
    std::unique_ptr<StrippedAbstraction> strip() const override {
        return std::make_unique<StrippedPDBAbstraction>(pattern, lookup_table);
    }
};
} // namespace simulations
