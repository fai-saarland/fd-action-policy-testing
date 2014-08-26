#include "globals.h"
#include "operator.h"

#include <iostream>
using namespace std;

Condition::Condition(istream &in) {
    in >> var >> val;
}

// TODO if the input file format was changed, we would need something like this
// Effect::Effect(istream &in) {
//    int condCount;
//    in >> condCount;
//    for (int i = 0; i < condCount; i++)
//        cond.push_back(Prevail(in));
//    in >> var >> pre >> post;
//}


void Operator::read_pre_post(istream &in) {
    int condCount, var, pre, post;
    in >> condCount;
    std::vector<Condition> conditions;
    for (int i = 0; i < condCount; i++)
        conditions.push_back(Condition(in));
    in >> var >> pre >> post;
    if (pre != -1)
        preconditions.push_back(Condition(var, pre));
    effects.push_back(Effect(var, post, conditions));
}


Operator::Operator(istream &in, bool axiom) {
    marked = false;

    is_an_axiom = axiom;
    if (!is_an_axiom) {
        check_magic(in, "begin_operator");
        in >> ws;
        getline(in, name);
        int count;
        in >> count;
        for (int i = 0; i < count; i++)
            preconditions.push_back(Condition(in));
        in >> count;
        for (int i = 0; i < count; i++)
            read_pre_post(in);

        int op_cost;
        in >> op_cost;
        cost = g_use_metric ? op_cost : 1;

        g_min_action_cost = min(g_min_action_cost, cost);
        g_max_action_cost = max(g_max_action_cost, cost);

        check_magic(in, "end_operator");
    } else {
        name = "<axiom>";
        cost = 0;
        check_magic(in, "begin_rule");
            read_pre_post(in);
        check_magic(in, "end_rule");
    }

    marker1 = marker2 = false;
}

void Condition::dump() const {
    cout << g_variable_name[var] << ": " << val;
}

void Effect::dump() const {
    cout << g_variable_name[var] << ":= " << val;
    if (!conditions.empty()) {
        cout << " if";
        for (int i = 0; i < conditions.size(); i++) {
            cout << " ";
            conditions[i].dump();
        }
    }
}

void Operator::dump() const {
    cout << name << ":";
    for (int i = 0; i < preconditions.size(); i++) {
        cout << " [";
        preconditions[i].dump();
        cout << "]";
    }
    for (int i = 0; i < effects.size(); i++) {
        cout << " [";
        effects[i].dump();
        cout << "]";
    }
    cout << endl;
}
