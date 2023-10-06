#include "atomic_unrelaxation_oracle.h"

#include <memory>

#include "../simulations/merge_and_shrink/abstraction_builder.h"

#include "../../plugin.h"

namespace policy_testing {
AtomicUnrelaxationOracle::AtomicUnrelaxationOracle(const options::Options &opts)
    : UnrelaxationOracle(opts) {
    if (!could_be_based_on_atomic_abstraction()) {
        throw std::logic_error("AtomicUnrelaxationOracle must be based on atomic abstraction builder.");
    }
}

void
AtomicUnrelaxationOracle::initialize() {
    if (initialized) {
        return;
    }
    UnrelaxationOracle::initialize();
    const unsigned int num_variables = simulations::global_simulation_task()->get_num_variables();
    possible_relaxations.resize(num_variables);
    possible_unrelaxations.resize(num_variables);
    for (int var = 0; var < num_variables; ++var) {
#define PRECOMPUTE_RELAXATIONS \
    const unsigned int domain_size = simulations::global_simulation_task()->get_variable_domain_size(var); \
    possible_relaxations[var].resize(domain_size); \
    possible_unrelaxations[var].resize(domain_size); \
    for (int s = 0; s < domain_size; ++s) { \
        for (int t = 0; t < domain_size; ++t) { \
            if (s == t) { \
                continue; \
            } \
            int dominance_value = local_dominance_relation.atomic_q_simulates(t, s); \
            if (dominance_value == simulations::MINUS_INFINITY) { \
                continue; \
            } \
            possible_relaxations[var][s].emplace_back(var, s, t, dominance_value); \
            possible_unrelaxations[var][t].emplace_back(var, s, t, dominance_value); \
        } \
    }
        if (read_simulation) {
            const auto &local_dominance_relation = stripped_numeric_dominance_relation->get_simulation_of_variable(var);
            PRECOMPUTE_RELAXATIONS
        } else {
            const auto &local_dominance_relation = numeric_dominance_relation->get_simulation_of_variable(var);
            PRECOMPUTE_RELAXATIONS
        }
    }
}


void
AtomicUnrelaxationOracle::add_options_to_parser(options::OptionParser &parser) {
    UnrelaxationOracle::add_options_to_parser(parser);
}

State
AtomicUnrelaxationOracle::relax(const State &s, const AtomicMetamorphicOption &o) const {
    assert(s[o.variable].get_value() == o.unrelaxed_value);
    assert(o.dominance_value > simulations::MINUS_INFINITY);
    s.unpack();
    std::vector<int> t = s.get_values();
    t[o.variable] = o.relaxed_value;
    return get_state_registry().insert_state(std::move(t));
}

State
AtomicUnrelaxationOracle::unrelax(const State &s, const AtomicMetamorphicOption &o) const {
    assert(s[o.variable].get_value() == o.relaxed_value);
    assert(o.dominance_value > simulations::MINUS_INFINITY);
    s.unpack();
    std::vector<int> t = s.get_values();
    t[o.variable] = o.unrelaxed_value;
    return get_state_registry().insert_state(std::move(t));
}

unsigned int AtomicUnrelaxationOracle::num_possible_relaxations(const State &s) const {
    unsigned int num_relaxation_candidates = 0;
    for (int var = 0; var < s.size(); ++var) {
        num_relaxation_candidates += possible_relaxations[var][s[var].get_value()].size();
    }
    return num_relaxation_candidates;
}

unsigned int AtomicUnrelaxationOracle::num_possible_unrelaxations(const State &s) const {
    unsigned int num_unrelaxation_candidates = 0;
    for (int var = 0; var < s.size(); ++var) {
        num_unrelaxation_candidates += possible_unrelaxations[var][s[var].get_value()].size();
    }
    return num_unrelaxation_candidates;
}

[[nodiscard]] AtomicMetamorphicOption AtomicUnrelaxationOracle::get_relaxation(const State &s,
                                                                               unsigned int index) const {
    assert(index < num_possible_relaxations(s));
    for (int var = 0; var < s.size(); ++var) {
        const auto &local_candidates = possible_relaxations[var][s[var].get_value()];
        const unsigned int num_local_candidates = local_candidates.size();
        if (index < num_local_candidates) {
            return local_candidates[index];
        }
        assert(index >= num_local_candidates);
        index -= num_local_candidates;
    }
    assert(false);
    return {};
}

[[nodiscard]] AtomicMetamorphicOption AtomicUnrelaxationOracle::get_unrelaxation(const State &s,
                                                                                 unsigned int index) const {
    assert(index < num_possible_unrelaxations(s));
    for (int var = 0; var < s.size(); ++var) {
        const auto &local_candidates = possible_unrelaxations[var][s[var].get_value()];
        const unsigned int num_local_candidates = local_candidates.size();
        if (index < num_local_candidates) {
            return local_candidates[index];
        }
        assert(index >= num_local_candidates);
        index -= num_local_candidates;
    }
    assert(false);
    return {};
}

std::vector<unsigned int> AtomicUnrelaxationOracle::pick_n_of_range(unsigned int n, unsigned int range) {
    std::vector<unsigned int> res(range);
    std::iota(res.begin(), res.end(), 0);
    simulations::simulations_rng.shuffle(res);
    res.resize(std::min(n, range));
    return res;
}


[[maybe_unused]] std::vector<std::pair<State, NumericDominanceOracle::DominanceValue>>
AtomicUnrelaxationOracle::relax(
    const State &s) const {
    const unsigned int num_relaxation_candidates = num_possible_relaxations(s);
    if (num_relaxation_candidates == 0 || operations_per_state == 1) {
        return {};
    }
    std::vector<std::pair<State, NumericDominanceOracle::DominanceValue>> result;
    if (operations_per_state == 1) {
        const int pick_index = simulations::simulations_rng(static_cast<int>(num_relaxation_candidates));
        AtomicMetamorphicOption pick = get_relaxation(s, pick_index);
        result.emplace_back(relax(s, pick), pick.dominance_value);
    } else {
        for (unsigned int pick_index : pick_n_of_range(operations_per_state, num_relaxation_candidates)) {
            AtomicMetamorphicOption pick = get_relaxation(s, pick_index);
            result.emplace_back(relax(s, pick), pick.dominance_value);
        }
    }
    assert(result.size() == std::min(operations_per_state, num_relaxation_candidates));
    if (debug_) {
        std::cout << "(Debug) Constructed  " << result.size() << ", relaxed states:" << std::endl;
        for (const auto &[state, dominance_value] : result) {
            std::cout << "(Debug) Relaxed state: " << state << ", dominance value: " << dominance_value <<
                std::endl;
        }
    }
    return result;
}

std::vector<std::pair<State, NumericDominanceOracle::DominanceValue>>
AtomicUnrelaxationOracle::unrelax(const State &s) const {
    const unsigned int num_unrelaxation_candidates = num_possible_unrelaxations(s);
    if (num_unrelaxation_candidates == 0 || operations_per_state == 0) {
        return {};
    }
    std::vector<std::pair<State, NumericDominanceOracle::DominanceValue>> result;
    if (operations_per_state == 1) {
        const int pick_index = simulations::simulations_rng(static_cast<int>(num_unrelaxation_candidates));
        AtomicMetamorphicOption pick = get_unrelaxation(s, pick_index);
        result.emplace_back(unrelax(s, pick), pick.dominance_value);
    } else {
        for (unsigned int pick_index : pick_n_of_range(operations_per_state, num_unrelaxation_candidates)) {
            AtomicMetamorphicOption pick = get_unrelaxation(s, pick_index);
            result.emplace_back(unrelax(s, pick), pick.dominance_value);
        }
    }
    assert(result.size() == std::min(operations_per_state, num_unrelaxation_candidates));
    if (debug_) {
        std::cout << "(Debug) Constructed  " << result.size() << " unrelaxed states:" << std::endl;
        for (const auto &[state, dominance_value] : result) {
            std::cout << "(Debug) " << state << " (dominance value: " << dominance_value << ")" << std::endl;
        }
    }
    return result;
}

void AtomicUnrelaxationOracle::print_debug_info() const {
    std::cout << "\n\nSummary of numeric dominance relation:" << std::endl;
    const unsigned int num_variables = simulations::global_simulation_task()->get_num_variables();
    std::cout << "Number of variables: " << num_variables << std::endl;
    for (int var = 0; var < num_variables; ++var) {
        int num_relaxations = 0;
        std::cout << "Possible relaxations for variable " << var << " (" <<
            simulations::global_simulation_task()->get_variable_name(var)
                  << "):\n(domain size = " << possible_relaxations[var].size() << ", values: {";
        bool first = true;
        for (int val = 0; val < possible_relaxations[var].size(); ++val) {
            if (first) {
                first = false;
            } else {
                std::cout << ", ";
            }
            std::cout << val << ": " << simulations::global_simulation_task()->get_fact_name(FactPair(var, val));
        }
        std::cout << "})" << std::endl;
        for (const auto &val : possible_relaxations[var]) {
            for (const AtomicMetamorphicOption &relax : val) {
                ++num_relaxations;
                std::cout << "\t" << relax << std::endl;
            }
        }
        std::cout << "Number of options: " << num_relaxations << "\n";
    }
    std::cout << std::endl;
}

std::ostream &operator<<(std::ostream &out, const AtomicMetamorphicOption &o) {
    return out << "D(" <<
           simulations::global_simulation_task()->get_fact_name(FactPair(o.variable, o.unrelaxed_value))
               << ", " <<
           simulations::global_simulation_task()->get_fact_name(FactPair(o.variable, o.relaxed_value))
               << ") = " << o.dominance_value;
}

static Plugin<Oracle>
_plugin("atomic_unrelaxation_oracle", options::parse<NumericDominanceOracle, AtomicUnrelaxationOracle>);
} // namespace policy_testing
