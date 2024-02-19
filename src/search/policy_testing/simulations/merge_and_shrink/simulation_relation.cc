#include "simulation_relation.h"

#include "label_relation.h"
#include "labelled_transition_system.h"

#include <iostream>
#include <list>
#include "abstraction.h"


namespace simulations {
SimulationRelation::SimulationRelation(Abstraction *_abs) : abs(_abs) {
    // set a pointer in the abstraction to this simulation relation
    abs->set_simulation_relation(this);
}


void SimulationRelation::init_goal_respecting() {
    int num_states = abs->size();
    const std::vector<bool> &goal_states = abs->get_goal_states();
    if (!abs->are_distances_computed()) {
        std::cerr <<
            "Error (init_goal_respecting): Distances must have been computed before creating the simulation relation!"
                  << std::endl;
        std::exit(-1);
    }
    const std::vector<int> &goal_distances = abs->get_goal_distances();
    relation.resize(num_states);
    for (int i = 0; i < num_states; i++) {
        relation[i].resize(num_states, true);
        if (!goal_states[i]) {
            for (int j = 0; j < num_states; j++) {
                if (goal_states[j] || goal_distances[i] > goal_distances[j]) {
                    //if (!goal_states[j]) cout << "BANG" << endl;
                    relation[i][j] = false;
                }
            }
        }
    }
}

bool SimulationRelation::simulates(const State &t, const State &s) const {
    int tid = abs->get_abstract_state(t);
    int sid = abs->get_abstract_state(s);
    return relation[tid][sid];
}


void SimulationRelation::init_identity() {
    for (int i = 0; i < relation.size(); i++) {
        for (int j = 0; j < relation[i].size(); j++) {
            relation[i][j] = (i == j);
        }
    }
}


void SimulationRelation::init_incremental(CompositeAbstraction *_abs,
                                          const SimulationRelation &simrel_one,
                                          const SimulationRelation &simrel_two) {
    assert(abs == _abs);

    int num_states = abs->size();
    assert(abs->are_distances_computed());
    init_goal_respecting();

    fixed_relation.resize(num_states);
    for (int i = 0; i < num_states; i++) {
        fixed_relation[i].resize(num_states, false);
    }

    int num_one = simrel_one.num_states();
    int num_two = simrel_two.num_states();

    for (int i = 0; i < num_one; i++) {
        for (int j = 0; j < num_one; j++) {
            if (simrel_one.simulates(i, j)) {
                for (int x = 0; x < num_two; x++) {
                    int ip = _abs->get_abstract_state(i, x);
                    if (ip == Abstraction::PRUNED_STATE)
                        continue;
                    for (int y = 0; y < num_two; y++) {
                        if (simrel_two.simulates(x, y)) {
                            int jp = _abs->get_abstract_state(j, y);
                            if (ip == jp || jp == Abstraction::PRUNED_STATE)
                                continue;
                            assert(!abs->is_goal_state(jp) || abs->is_goal_state(ip));
                            relation[ip][jp] = true;
                            fixed_relation[ip][jp] = true;
                        }
                    }
                }
            }
        }
    }
}

void SimulationRelation::apply_shrinking_to_table(const std::vector<int> &abstraction_mapping) {
    std::cout << "reducing simulation size from " << relation.size() << " to " << abs->size() << std::endl;
    int new_states = abs->size();
    std::vector<std::vector<bool>> new_relation(new_states);
    for (int i = 0; i < new_states; i++) {
        new_relation[i].resize(new_states, false);
    }
    int old_states = abstraction_mapping.size();
    for (int i = 0; i < old_states; i++) {
        int new_i = abstraction_mapping[i];
        if (new_i == Abstraction::PRUNED_STATE)
            continue;
        for (int j = 0; j < old_states; j++) {
            int new_j = abstraction_mapping[j];
            if (new_j == Abstraction::PRUNED_STATE)
                continue;
            new_relation[new_i][new_j] = relation[i][j];
        }
    }
    std::vector<std::vector<bool>>().swap(relation);
    relation.swap(new_relation);
}

void SimulationRelation::reset() {
    int num_states = abs->size();
    const std::vector<bool> &goal_states = abs->get_goal_states();
    fixed_relation.resize(num_states);
    for (int i = 0; i < num_states; i++) {
        fixed_relation[i].resize(num_states, false);
        for (int j = 0; j < num_states; j++) {
            if (relation[i][j]) {
                fixed_relation[i][j] = true;
            } else if (goal_states[i] || !goal_states[j]) {
                relation[i][j] = true;
            }
        }
    }
}

void SimulationRelation::dump(const std::vector <std::string> &names) const {
    std::cout << "SIMREL:" << std::endl;
    for (int j = 0; j < relation.size(); ++j) {
        for (int i = 0; i < relation.size(); ++i) {
            if (simulates(j, i) && i != j) {
                if (simulates(i, j)) {
                    if (j < i) {
                        std::cout << names[i] << " <=> " << names[j] << std::endl;
                    }
                } else {
                    std::cout << names[i] << " <= " << names[j] << std::endl;
                }
            }
        }
    }
}


void SimulationRelation::dump() const {
    std::cout << "SIMREL:" << std::endl;
    for (int j = 0; j < relation.size(); ++j) {
        for (int i = 0; i < relation.size(); ++i) {
            if (simulates(j, i) && i != j) {
                if (simulates(i, j)) {
                    if (j < i) {
                        std::cout << i << " <=> " << j << std::endl;
                    }
                } else {
                    std::cout << i << " <= " << j << std::endl;
                }
            }
        }
    }
}


int SimulationRelation::num_equivalences() const {
    int num = 0;
    std::vector<bool> counted(relation.size(), false);
    for (int i = 0; i < counted.size(); i++) {
        if (!counted[i]) {
            for (int j = i + 1; j < relation.size(); j++) {
                if (similar(i, j)) {
                    counted[j] = true;
                }
            }
        } else {
            num++;
        }
    }
    return num;
}

bool SimulationRelation::is_identity() const {
    for (int i = 0; i < relation.size(); ++i) {
        for (int j = i + 1; j < relation.size(); ++j) {
            if (simulates(i, j) || simulates(j, i)) {
                return false;
            }
        }
    }
    return true;
}


int SimulationRelation::num_simulations(bool ignore_equivalences) const {
    int res = 0;
    if (ignore_equivalences) {
        std::vector<bool> counted(relation.size(), false);
        for (int i = 0; i < relation.size(); ++i) {
            if (!counted[i]) {
                for (int j = i + 1; j < relation.size(); ++j) {
                    if (similar(i, j)) {
                        counted[j] = true;
                    }
                }
            }
        }
        for (int i = 0; i < relation.size(); ++i) {
            if (!counted[i]) {
                for (int j = i + 1; j < relation.size(); ++j) {
                    if (!counted[j]) {
                        if (!similar(i, j) && (simulates(i, j) || simulates(j, i))) {
                            res++;
                        }
                    }
                }
            }
        }
    } else {
        for (int i = 0; i < relation.size(); ++i)
            for (int j = 0; j < relation.size(); ++j)
                if (simulates(i, j))
                    res++;
    }
    return res;
}

//Computes the probability of selecting a random pair s, s' such that
//s is equivalent to s'.
double SimulationRelation::get_percentage_equivalences() const {
    double num_eq = 0;
    double num_states = relation.size();
    for (int i = 0; i < relation.size(); ++i)
        for (int j = 0; j < relation.size(); ++j)
            if (similar(i, j))
                num_eq++;
    return num_eq / (num_states * num_states);
}

void SimulationRelation::shrink() {
    std::vector<std::list<int>> equivRel;
    equivRel.reserve(relation.size());
    std::vector<bool> already_in(relation.size(), false);
    for (int i = 0; i < already_in.size(); i++) {
        // was already added due to being similar to some other state
        if (already_in[i])
            continue;
        already_in[i] = true;
        std::list<int> newEquiv;
        newEquiv.push_front(i);
        for (int j = i + 1; j < relation.size(); j++) {
            if (similar(i, j)) {
                already_in[j] = true;
                newEquiv.push_front(j);
            }
        }
        equivRel.push_back(newEquiv);
    }
    if (abs->size() != equivRel.size()) {
        std::cout <<
            "Size for applying simulation shrinking: " << equivRel.size() << "; was: " << abs->size() << std::endl;
        abs->apply_abstraction(equivRel);
        abs->normalize();
    } else {
        std::cout << "Simulation shrinking did not shrink anything" << std::endl;
    }
}


void SimulationRelation::compute_list_dominated_states() {
    dominated_states.resize(relation.size());
    dominating_states.resize(relation.size());

    for (int s = 0; s < relation.size(); ++s) {
        for (int t = 0; t < relation.size(); ++t) {
            if (simulates(t, s)) {
                dominated_states[t].push_back(s);
                dominating_states[s].push_back(t);
            }
        }
    }
}

const std::vector<int> &SimulationRelation::get_varset() const {
    return abs->get_varset();
}

bool SimulationRelation::pruned(const State &state) const {
    return abs->get_abstract_state(state) == -1;
}

int SimulationRelation::get_cost(const State &state) const {
    return abs->get_cost(state);
}

int SimulationRelation::get_index(const State &state) const {
    return abs->get_abstract_state(state);
}

bool SimulationRelation::has_positive_dominance() const {
    for (int j = 0; j < relation.size(); ++j) {
        for (int i = 0; i < relation.size(); ++i) {
            if (i == j)
                continue;
            if (relation[i][j]) {
                return true;
            }
        }
    }

    return false;
}


std::vector<int> SimulationRelation::get_dangerous_labels(const LabelledTransitionSystem *lts) const {
    std::vector<int> dangerous_labels;

    int num_states = lts->size();
    std::vector<bool> is_state_to_check(num_states);
    std::vector<bool> is_ok(num_states);
    for (LabelGroup lg(0); lg.group < lts->get_num_label_groups(); ++lg) {
        std::fill(is_ok.begin(), is_ok.end(), false);
        std::fill(is_state_to_check.begin(), is_state_to_check.end(), false);
        const std::vector<TSTransition> &trs = lts->get_transitions_label_group(lg);
        std::vector<int> states_to_check;

        for (const auto &tr: trs) {
            int s = tr.src;
            int t = tr.target;
            if (is_ok[s]) {
                continue;
            } else if (simulates(t, s)) {
                is_ok[s] = true;
            } else if (!is_state_to_check[s]) {
                states_to_check.push_back(s);
                is_state_to_check[s] = true;
            }
        }

        for (int s: states_to_check) {
            if (!is_ok[s]) {
                // Add dangerous labels
                const std::vector<int> &labels = lts->get_labels(lg);
                dangerous_labels.insert(dangerous_labels.end(), labels.begin(), labels.end());
                break;
            }
        }
    }
    //for (int i  : dangerous_labels) cout << g_operators[i].get_name() << endl;
    return dangerous_labels;
}

const std::vector<int> &SimulationRelation::get_dominated_states(const State &state) {
    if (dominated_states.empty()) {
        compute_list_dominated_states();
    }
    return dominated_states[abs->get_abstract_state(state)];
}

const std::vector<int> &SimulationRelation::get_dominating_states(const State &state) {
    if (dominated_states.empty()) {
        compute_list_dominated_states();
    }
    return dominating_states[abs->get_abstract_state(state)];
}


void SimulationRelation::cancel_simulation_computation() {
    std::vector<std::vector<bool>>().swap(relation);
}
}
