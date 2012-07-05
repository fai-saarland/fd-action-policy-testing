#include "abstract_state.h"

#include "utils.h"
#include "../globals.h"
#include "../operator.h"
#include "../state.h"

#include <algorithm>
#include <cassert>
#include <set>
#include <sstream>
#include <utility>
#include <vector>

using namespace std;

namespace cegar_heuristic {
int same = 0;
int different = 0;
int doubles = 0;

AbstractState::AbstractState(string s, bool init) {
    assert(!g_variable_domain.empty());
    prev_arc = 0;
    next_arc = 0;
    min_distance = 0;

    values.resize(g_variable_domain.size(), set<int>());

    if (init)
        for (int var = 0; var < g_variable_domain.size(); ++var) {
            for (int value = 0; value < g_variable_domain[var]; ++value)
                values[var].insert(values[var].end(), value);
        }

    if (s.empty())
        return;

    // Construct state from string s of the form "<0={0,1}>".
    istringstream iss(s);
    char next;
    int var;
    int val;
    bool in_bracket = false;
    while (!iss.eof()) {
        // iss.peek() and iss.get() return strange chars at beginning and end
        // of stream.
        iss >> next;
        if (next == '<' || next == '>' || next == '=' || next == ',') {
            // skip.
        } else if (next == '{') {
            in_bracket = true;
        } else if (next == '}') {
            in_bracket = false;
        } else if ((next >= '0') && (next <= '9')) {
            // We can use iss.unget(), because next is a single char.
            iss.unget();
            if (in_bracket) {
                iss >> val;
                values[var].insert(val);
            } else {
                iss >> var;
                values[var].clear();
            }
        }
    }
}

string AbstractState::str() const {
    ostringstream oss;
    string sep = "";
    oss << "<";
    for (int i = 0; i < values.size(); ++i) {
        set<int> vals = values[i];
        if (vals.size() != g_variable_domain[i]) {
            oss << sep << i << "=" << int_set_to_string(vals);
            sep = ",";
        }
    }
    oss << ">";
    return oss.str();
}

string AbstractState::get_next_as_string() const {
    // Format: [(op.name,state.str),...]
    ostringstream oss;
    string sep = "";
    oss << "[";
    for (int i = 0; i < next.size(); ++i) {
        Operator *op = next[i].first;
        AbstractState *abs = next[i].second;
        oss << sep << "(" << op->get_name() << "," << abs->str() << ")";
        sep = ",";
    }
    oss << "]";
    return oss.str();
}

bool AbstractState::operator==(const AbstractState &other) const {
    return values == other.values;
}

bool AbstractState::operator!=(const AbstractState &other) const {
    return !(*this == other);
}

const Domain &AbstractState::get_values(int var) const {
    return values[var];
}

void AbstractState::set_value(int var, int value) {
    values[var].clear();
    values[var].insert(value);
}

void AbstractState::regress(const Operator &op, AbstractState *result) const {
    for (int v = 0; v < g_variable_domain.size(); ++v) {
        set<int> s1_vals;
        // s2_vals = s2[v]
        set<int> s2_vals = get_values(v);
        // if v occurs in op.eff:
        int eff = get_eff(op, v);
        if (eff != UNDEFINED) {
            assert(s2_vals.count(eff) == 1);
            // s1_vals = v.all_values_in_domain
            for (int i = 0; i < g_variable_domain[v]; ++i)
                s1_vals.insert(i);
        } else {
            // s1_vals = s2_vals
            s1_vals = s2_vals;
        }
        // if v occurs in op.pre:
        int pre = get_pre(op, v);
        if (pre != UNDEFINED) {
            assert(s1_vals.count(pre) == 1);
            // s1_vals = [op.pre[v]]
            s1_vals.clear();
            s1_vals.insert(pre);
        }
        result->values[v] = s1_vals;
    }
}

void AbstractState::get_unmet_conditions(const AbstractState &desired,
                                         vector<pair<int, int> > *conditions)
const {
    // Get all set intersections of the possible values here minus the possible
    // values in "desired".
    for (int i = 0; i < g_variable_domain.size(); ++i) {
        vector<int> both(g_variable_domain[i]);
        vector<int>::iterator it;
        set<int> vals1 = this->get_values(i);
        set<int> vals2 = desired.get_values(i);
        it = set_intersection(vals1.begin(), vals1.end(),
                              vals2.begin(), vals2.end(), both.begin());
        int elements = static_cast<int>(it - both.begin());
        assert(elements > 0);
        if (elements < vals1.size()) {
            // The variable's value matters for determining the resulting state.
            for (int j = 0; j < elements; ++j) {
                conditions->push_back(pair<int, int>(i, both[j]));
            }
        }
    }
}

AbstractState *AbstractState::refine(int var, int value, AbstractState *v1, AbstractState *v2) {
    // We can only refine for vars that can have at least two values.
    assert(get_values(var).size() >= 2);
    // The desired value has to be in the set of possible values.
    assert(get_values(var).count(value) == 1);

    v1->values = values;
    v2->values = values;

    // In v1 var can have all of the previous values except the desired one.
    v1->values[var].erase(value);

    // In v2 var can only have the desired value.
    v2->set_value(var, value);

    // Results from Dijkstra search. If  u --> v --> w  was on the
    // shortest path and a new path  u --> v{1,2} --> w is created with the
    // same arcs, we avoid a dijkstra computation.
    Operator *op_in = (prev_arc) ? prev_arc->first : 0;
    AbstractState *state_in = (prev_arc) ? prev_arc->second : 0;
    Operator *op_out = (next_arc) ? next_arc->first : 0;
    AbstractState *state_out = (next_arc) ? next_arc->second : 0;
    bool u_v1 = false, u_v2 = false, v1_w = false, v2_w = false;

    // Before: u --> this=v --> w
    //  ==>
    // After:  v is split into v1 and v2
    for (int i = 0; i < prev.size(); ++i) {
        Operator *op = prev[i].first;
        AbstractState *u = prev[i].second;
        if (u != this) {
            assert(*u != *this);
            u->remove_next_arc(op, this);
            // If the first check returns false, the second arc has to be added.
            if (u->check_and_add_arc(op, v1)) {
                bool added = u->check_and_add_arc(op, v2);
                if (op == op_in && u == state_in) {
                    u_v1 = true;
                    u_v2 |= added;
                }
            } else {
                u->add_arc(op, v2);
                u_v2 |= (op == op_in && u == state_in);
            }
        }
    }
    for (int i = 0; i < next.size(); ++i) {
        Operator *op = next[i].first;
        AbstractState *w = next[i].second;
        if (w == this) {
            assert(*w == *this);
            // Handle former self-loops. The same loops also were in prev,
            // but they only have to be checked once.
            v1->check_and_add_arc(op, v2);
            v2->check_and_add_arc(op, v1);
            v1->check_and_add_arc(op, v1);
            v2->check_and_add_arc(op, v2);
        } else {
            w->remove_prev_arc(op, this);
            // If the first check returns false, the second arc has to be added.
            if (v1->check_and_add_arc(op, w)) {
                bool added = v2->check_and_add_arc(op, w);
                if (op == op_out && w == state_out) {
                    v1_w = true;
                    v2_w |= added;
                }
            } else {
                v2->add_arc(op, w);
                v2_w |= (op == op_out && w == state_out);
            }
        }
    }
    // Save the refinement hierarchy.
    this->var = var;
    Domain values = v1->get_values(var);
    for (Domain::iterator it = values.begin(); it != values.end(); ++it)
        children[*it] = v1;
    assert(v1->get_values(var).size() == this->get_values(var).size() - 1);
    assert(v2->get_values(var).size() == 1);
    children[value] = v2;
    left = v1;
    right = v2;

    // Check that the sets of possible values are now smaller.
    assert(this->is_abstraction_of(*v1));
    assert(this->is_abstraction_of(*v2));
    assert(*this != *v1);
    assert(*this != *v2);

    // Remove obsolete members.
    vector<Arc>().swap(next);
    vector<Arc>().swap(prev);
    vector<Domain>().swap(this->values);
    delete this->prev_arc;
    delete this->next_arc;

    // Pass on the h-value.
    v1->set_min_distance(min_distance);
    v2->set_min_distance(min_distance);

    AbstractState *bridge_state = 0;
    // If we refine a goal state, only reuse solution if the path leads to the goal.
    bool v1_is_bridge = ((u_v1 && v1_w) || (u_v1 && !state_out && v1->is_abstraction_of_goal()));
    if ((u_v2 && v2_w) || (u_v2 && !state_out && v2->is_abstraction_of_goal())) {
        // Prefer going over v2. // TODO: add option?
        if (v1_is_bridge)
            ++doubles;
        bridge_state = v2;
    } else if (v1_is_bridge) {
        bridge_state = v1;
    }
    if (bridge_state) {
        if (state_in) {
            state_in->set_next_arc(new Arc(op_in, bridge_state));
            if (state_out)
                bridge_state->set_next_arc(new Arc(op_out, state_out));
        }

    }
    if (bridge_state) {
        assert(state_in);
        ++same;
        return state_in;
    }
    ++different;
    return 0;
}

void AbstractState::add_arc(Operator *op, AbstractState *other) {
    next.push_back(Arc(op, other));
    other->prev.push_back(Arc(op, this));
}

void AbstractState::remove_arc(vector<Arc> *arcs, Operator *op, AbstractState *other) {
    for (int i = 0; i < arcs->size(); ++i) {
        Operator *current_op = (*arcs)[i].first;
        AbstractState *current_state = (*arcs)[i].second;
        if ((current_op == op) && (current_state == other)) {
            arcs->erase(arcs->begin() + i);
            return;
        }
    }
    // Do not try to remove an operator that is not there.
    cout << "REMOVE: " << str() << " " << op->get_name() << " " << other->str() << endl;
    assert(false);
}

void AbstractState::remove_next_arc(Operator *op, AbstractState *other) {
    remove_arc(&next, op, other);
}

void AbstractState::remove_prev_arc(Operator *op, AbstractState *other) {
    remove_arc(&prev, op, other);
}

bool AbstractState::check_arc(Operator *op, AbstractState *other) {
    //if (DEBUG)
    //    cout << "CHECK ARC: " << str() << " " << op->get_name() << " " << other->str() << endl;
    if (!applicable(*op))
        return false;
    AbstractState result = AbstractState("", false);
    apply(*op, &result);
    if (result.agrees_with(*other)) {
        add_arc(op, other);
        return true;
    }
    return false;
}

bool AbstractState::check_and_add_arc(Operator *op, AbstractState *other) {
    //if (DEBUG)
    //    cout << "CHECK ARC: " << str() << " " << op->get_name() << " " << other->str() << endl;
    vector<bool> checked(g_variable_domain.size(), false);
    for (int i = 0; i < op->get_prevail().size(); ++i) {
        const Prevail &prevail = op->get_prevail()[i];
        const int &var = prevail.var;
        const int &value = prevail.prev;
        // Check if operator is applicable.
        assert(value != -1);
        if (this->get_values(var).count(value) == 0)
            return false;
        // Check if we land in the desired state.
        // If this == other we have already done the check above.
        if ((this != other) && (other->get_values(var).count(value) == 0))
            return false;
        checked[var] = true;
    }
    for (int i = 0; i < op->get_pre_post().size(); ++i) {
        // Check if pre value is in the set of possible values.
        const PrePost &prepost = op->get_pre_post()[i];
        const int &var = prepost.var;
        const int &pre = prepost.pre;
        const int &post = prepost.post;
        assert(prepost.cond.empty());
        assert(!checked[var]);
        // Check if operator is applicable.
        if ((pre != -1) && (get_values(var).count(pre) == 0))
            return false;
        // Check if we land in the desired state.
        if (other->get_values(var).count(post) == 0)
            return false;
        checked[var] = true;
    }
    for (int var = 0; var < g_variable_domain.size(); ++var) {
        if (checked[var] || (this == other))
            continue;
        if (intersection_empty(this->get_values(var), other->get_values(var)))
            return false;
    }
    add_arc(op, other);
    return true;
}

bool AbstractState::applicable(const Operator &op) const {
    vector<pair<int, int> > preconditions;
    get_prevail_and_preconditions(op, &preconditions);
    for (int i = 0; i < preconditions.size(); ++i) {
        // Check if precondition value is in the set of possible values.
        int &var = preconditions[i].first;
        int &value = preconditions[i].second;
        // Only check value if it isn't -1.
        if ((value != -1) && (get_values(var).count(value) == 0))
            return false;
    }
    return true;
}

void AbstractState::apply(const Operator &op, AbstractState *result) const {
    assert(applicable(op));
    result->values = this->values;
    for (int i = 0; i < op.get_prevail().size(); ++i) {
        const Prevail &prevail = op.get_prevail()[i];
        result->set_value(prevail.var, prevail.prev);
    }
    for (int i = 0; i < op.get_pre_post().size(); ++i) {
        const PrePost &prepost = op.get_pre_post()[i];
        result->set_value(prepost.var, prepost.post);
    }
}

bool AbstractState::agrees_with(const AbstractState &other) const {
    // Two abstract states agree if for all variables the sets of possible
    // values sets have a non-empty intersection.
    for (int i = 0; i < g_variable_domain.size(); ++i) {
        vector<int> both(g_variable_domain[i]);
        vector<int>::iterator it;
        const Domain &vals1 = this->get_values(i);
        const Domain &vals2 = other.get_values(i);
        it = set_intersection(vals1.begin(), vals1.end(),
                              vals2.begin(), vals2.end(), both.begin());
        if (it == both.begin())
            // Set is empty.
            return false;
    }
    return true;
}

bool AbstractState::is_abstraction_of(const State &conc_state) const {
    // Return true if every concrete value is contained in the possible values.
    for (int i = 0; i < g_variable_domain.size(); ++i) {
        if (get_values(i).count(conc_state[i]) == 0)
            return false;
    }
    return true;
}

bool AbstractState::is_abstraction_of(const AbstractState &other) const {
    // Return true if all our possible value sets are supersets of the
    // other's respective sets.
    for (int i = 0; i < g_variable_domain.size(); ++i) {
        vector<int> diff(g_variable_domain[i]);
        vector<int>::iterator it;
        set<int> vals1 = this->get_values(i);
        set<int> vals2 = other.get_values(i);
        // If |vals2 - vals1| == 0, vals1 is a superset of vals2.
        it = set_difference(vals2.begin(), vals2.end(),
                            vals1.begin(), vals1.end(), diff.begin());
        if (it != diff.begin())
            // Set is not empty.
            return false;
    }
    return true;
}

bool AbstractState::is_abstraction_of_goal() const {
    assert(!g_goal.empty());
    for (int i = 0; i < g_goal.size(); ++i) {
        if (values[g_goal[i].first].count(g_goal[i].second) == 0) {
            return false;
        }
    }
    return true;
}

bool AbstractState::valid() const {
    return children.empty();
}

int AbstractState::get_var() const {
    return var;
}

AbstractState *AbstractState::get_child(int value) {
    return children[value];
}

AbstractState *AbstractState::get_left_child() const {
    return left;
}

AbstractState *AbstractState::get_right_child() const {
    return right;
}
}
