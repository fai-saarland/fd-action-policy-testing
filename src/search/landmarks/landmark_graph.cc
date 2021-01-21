#include "landmark_graph.h"

#include "util.h"

#include "../task_proxy.h"

#include "../utils/logging.h"
#include "../utils/memory.h"

#include <cassert>
#include <list>
#include <map>
#include <set>
#include <sstream>
#include <vector>

using namespace std;

namespace landmarks {
LandmarkGraph::LandmarkGraph(const TaskProxy &task_proxy)
    : conj_lms(0), disj_lms(0), task_proxy(task_proxy) {
}

LandmarkNode *LandmarkGraph::get_landmark(const FactPair &fact) const {
    /* Return pointer to landmark node that corresponds to the given fact, or 0 if no such
     landmark exists.
     */
    LandmarkNode *node_p = nullptr;
    auto it = simple_lms_to_nodes.find(fact);
    if (it != simple_lms_to_nodes.end())
        node_p = it->second;
    else {
        auto it2 = disj_lms_to_nodes.find(fact);
        if (it2 != disj_lms_to_nodes.end())
            node_p = it2->second;
    }
    return node_p;
}

LandmarkNode *LandmarkGraph::get_lm_for_index(int i) const {
    return nodes[i].get();
}

int LandmarkGraph::number_of_edges() const {
    int total = 0;
    for (auto &node : nodes)
        total += node->children.size();
    return total;
}

bool LandmarkGraph::simple_landmark_exists(const FactPair &lm) const {
    auto it = simple_lms_to_nodes.find(lm);
    assert(it == simple_lms_to_nodes.end() || !it->second->disjunctive);
    return it != simple_lms_to_nodes.end();
}

bool LandmarkGraph::landmark_exists(const FactPair &lm) const {
    // Note: this only checks for one fact whether it's part of a landmark, hence only
    // simple and disjunctive landmarks are checked.
    set<FactPair> lm_set;
    lm_set.insert(lm);
    return simple_landmark_exists(lm) || disj_landmark_exists(lm_set);
}

bool LandmarkGraph::disj_landmark_exists(const set<FactPair> &lm) const {
    // Test whether ONE of the facts in lm is present in some disj. LM
    for (const FactPair &lm_fact : lm) {
        if (disj_lms_to_nodes.count(lm_fact) == 1)
            return true;
    }
    return false;
}

bool LandmarkGraph::exact_same_disj_landmark_exists(const set<FactPair> &lm) const {
    // Test whether a disj. LM exists which consists EXACTLY of those facts in lm
    LandmarkNode *lmn = nullptr;
    for (const FactPair &lm_fact : lm) {
        auto it2 = disj_lms_to_nodes.find(lm_fact);
        if (it2 == disj_lms_to_nodes.end())
            return false;
        else {
            if (lmn && lmn != it2->second) {
                return false;
            } else if (!lmn)
                lmn = it2->second;
        }
    }
    return true;
}

LandmarkNode &LandmarkGraph::landmark_add_simple(const FactPair &lm) {
    assert(!landmark_exists(lm));
    vector<FactPair> facts;
    facts.push_back(lm);
    unique_ptr<LandmarkNode> new_node = utils::make_unique_ptr<LandmarkNode>(facts, false);
    LandmarkNode *new_node_p = new_node.get();
    nodes.push_back(move(new_node));
    simple_lms_to_nodes.emplace(lm, new_node_p);
    return *new_node_p;
}

LandmarkNode &LandmarkGraph::landmark_add_disjunctive(const set<FactPair> &lm) {
    vector<FactPair> facts;
    for (const FactPair &lm_fact : lm) {
        facts.push_back(lm_fact);
        assert(!landmark_exists(lm_fact));
    }
    unique_ptr<LandmarkNode> new_node = utils::make_unique_ptr<LandmarkNode>(facts, true);
    LandmarkNode *new_node_p = new_node.get();
    nodes.push_back(move(new_node));
    for (const FactPair &lm_fact : lm) {
        disj_lms_to_nodes.emplace(lm_fact, new_node_p);
    }
    ++disj_lms;
    return *new_node_p;
}

LandmarkNode &LandmarkGraph::landmark_add_conjunctive(const set<FactPair> &lm) {
    vector<FactPair> facts;
    for (const FactPair &lm_fact : lm) {
        facts.push_back(lm_fact);
        assert(!landmark_exists(lm_fact));
    }
    unique_ptr<LandmarkNode> new_node = utils::make_unique_ptr<LandmarkNode>(facts, false, true);
    LandmarkNode *new_node_p = new_node.get();
    nodes.push_back(move(new_node));
    ++conj_lms;
    return *new_node_p;
}

void LandmarkGraph::remove_node_occurrences(LandmarkNode *node) {
    for (const auto &parent : node->parents) {
        LandmarkNode &parent_node = *(parent.first);
        parent_node.children.erase(node);
        assert(parent_node.children.find(node) == parent_node.children.end());
    }
    for (const auto &child : node->children) {
        LandmarkNode &child_node = *(child.first);
        child_node.parents.erase(node);
        assert(child_node.parents.find(node) == child_node.parents.end());
    }
    if (node->disjunctive) {
        --disj_lms;
        for (const FactPair &lm_fact : node->facts) {
            disj_lms_to_nodes.erase(lm_fact);
        }
    } else if (node->conjunctive) {
        --conj_lms;
    } else {
        simple_lms_to_nodes.erase(node->facts[0]);
    }
}

void LandmarkGraph::remove_node_if(
    const function<bool (const LandmarkNode &)> &remove_node) {
    for (auto &node : nodes) {
        if (remove_node(*node)) {
            remove_node_occurrences(node.get());
        }
    }
    nodes.erase(remove_if(nodes.begin(), nodes.end(),
                          [&remove_node](const unique_ptr<LandmarkNode> &node) {
                              return remove_node(*node);
                          }), nodes.end());
}

LandmarkNode &LandmarkGraph::make_disj_node_simple(const FactPair &lm) {
    LandmarkNode &node = get_disj_lm_node(lm);
    node.disjunctive = false;
    for (const FactPair &lm_fact : node.facts)
        disj_lms_to_nodes.erase(lm_fact);
    simple_lms_to_nodes.emplace(lm, &node);
    return node;
}

void LandmarkGraph::set_landmark_ids() {
    int id = 0;
    for (auto &lmn : nodes) {
        lmn->assign_id(id);
        ++id;
    }
}

void LandmarkGraph::dump_node(const LandmarkNode *node_p) const {
    const VariablesProxy variables = task_proxy.get_variables();
    utils::g_log << "LM " << node_p->get_id() << " ";
    if (node_p->disjunctive)
        utils::g_log << "disj {";
    else if (node_p->conjunctive)
        utils::g_log << "conj {";
    size_t i = 0;
    for (const FactPair &lm_fact : node_p->facts) {
        VariableProxy var = variables[lm_fact.var];
        utils::g_log << var.get_fact(lm_fact.value).get_name() << " ("
                     << var.get_name() << "(" << lm_fact.var << ")"
                     << "->" << lm_fact.value << ")";
        if (i++ < node_p->facts.size() - 1)
            utils::g_log << ", ";
    }
    if (node_p->disjunctive || node_p->conjunctive)
        utils::g_log << "}";
    if (node_p->in_goal)
        utils::g_log << "(goal)";
    utils::g_log << " Achievers (" << node_p->possible_achievers.size() << ", " << node_p->first_achievers.size() << ")";
    utils::g_log << endl;
}

void LandmarkGraph::dump() const {
    utils::g_log << "Landmark graph: " << endl;

    for (const unique_ptr<LandmarkNode> &node : nodes) {
        dump_node(node.get());
        for (const auto &parent : node->parents) {
            const LandmarkNode *parent_node = parent.first;
            const EdgeType &edge = parent.second;
            utils::g_log << "\t\t<-_";
            switch (edge) {
            case EdgeType::necessary:
                utils::g_log << "nec ";
                break;
            case EdgeType::greedy_necessary:
                utils::g_log << "gn  ";
                break;
            case EdgeType::natural:
                utils::g_log << "nat ";
                break;
            case EdgeType::reasonable:
                utils::g_log << "r   ";
                break;
            case EdgeType::obedient_reasonable:
                utils::g_log << "o_r ";
                break;
            }
            dump_node(parent_node);
        }
        for (const auto &child : node->children) {
            const LandmarkNode *child_node = child.first;
            const EdgeType &edge = child.second;
            utils::g_log << "\t\t->_";
            switch (edge) {
            case EdgeType::necessary:
                utils::g_log << "nec ";
                break;
            case EdgeType::greedy_necessary:
                utils::g_log << "gn  ";
                break;
            case EdgeType::natural:
                utils::g_log << "nat ";
                break;
            case EdgeType::reasonable:
                utils::g_log << "r   ";
                break;
            case EdgeType::obedient_reasonable:
                utils::g_log << "o_r ";
                break;
            }
            dump_node(child_node);
        }
        utils::g_log << endl;
    }
    utils::g_log << "Landmark graph end." << endl;
}
}
