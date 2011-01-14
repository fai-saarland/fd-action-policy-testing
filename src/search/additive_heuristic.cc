#include "additive_heuristic.h"

#include "operator.h"
#include "option_parser.h"
#include "plugin.h"
#include "state.h"

#include <cassert>
#include <vector>
using namespace std;


static ScalarEvaluatorPlugin additive_heuristic_plugin(
    "add", AdditiveHeuristic::create);


// construction and destruction
AdditiveHeuristic::AdditiveHeuristic(const HeuristicOptions &options)
    : RelaxationHeuristic(options) {
}

AdditiveHeuristic::~AdditiveHeuristic() {
}

// initialization
void AdditiveHeuristic::initialize() {
    cout << "Initializing additive heuristic..." << endl;
    RelaxationHeuristic::initialize();
}

// heuristic computation
void AdditiveHeuristic::setup_exploration_queue() {
    queue.clear();

    for (int var = 0; var < propositions.size(); var++) {
        for (int value = 0; value < propositions[var].size(); value++) {
            Proposition &prop = propositions[var][value];
            prop.cost = -1;
            prop.marked = false;
        }
    }

    // Deal with operators and axioms without preconditions.
    for (int i = 0; i < unary_operators.size(); i++) {
        UnaryOperator &op = unary_operators[i];
        op.unsatisfied_preconditions = op.precondition.size();
        op.cost = op.base_cost; // will be increased by precondition costs

        if (op.unsatisfied_preconditions == 0)
            enqueue_if_necessary(op.effect, op.base_cost, &op);
    }
}

void AdditiveHeuristic::setup_exploration_queue_state(const State &state) {
    for (int var = 0; var < propositions.size(); var++) {
        Proposition *init_prop = &propositions[var][state[var]];
        enqueue_if_necessary(init_prop, 0, 0);
    }
}

void AdditiveHeuristic::relaxed_exploration() {
    int unsolved_goals = goal_propositions.size();
    while (!queue.empty()) {
        pair<int, Proposition *> top_pair = queue.pop();
        int distance = top_pair.first;
        Proposition *prop = top_pair.second;
        int prop_cost = prop->cost;
        assert(prop_cost <= distance);
        if (prop_cost < distance)
            continue;
        if (prop->is_goal && --unsolved_goals == 0)
            return;
        const vector<UnaryOperator *> &triggered_operators =
            prop->precondition_of;
        for (int i = 0; i < triggered_operators.size(); i++) {
            UnaryOperator *unary_op = triggered_operators[i];
            unary_op->unsatisfied_preconditions--;
            unary_op->cost += prop_cost;
            assert(unary_op->unsatisfied_preconditions >= 0);
            if (unary_op->unsatisfied_preconditions == 0)
                enqueue_if_necessary(unary_op->effect,
                                     unary_op->cost, unary_op);
        }
    }
}

void AdditiveHeuristic::mark_preferred_operators(
    const State &state, Proposition *goal) {
    if (!goal->marked) { // Only consider each subgoal once.
        goal->marked = true;
        UnaryOperator *unary_op = goal->reached_by;
        if (unary_op) { // We have not yet chained back to a start node.
            for (int i = 0; i < unary_op->precondition.size(); i++)
                mark_preferred_operators(state, unary_op->precondition[i]);
            int operator_no = unary_op->operator_no;
            if (unary_op->cost == unary_op->base_cost && operator_no != -1) {
                // Necessary condition for this being a preferred
                // operator, which we use as a quick test before the
                // more expensive applicability test.
                // If we had no 0-cost operators and axioms to worry
                // about, this would also be a sufficient condition.
                const Operator *op = &g_operators[operator_no];
                if (op->is_applicable(state))
                    set_preferred(op);
            }
        }
    }
}

int AdditiveHeuristic::compute_add_and_ff(const State &state) {
    setup_exploration_queue();
    setup_exploration_queue_state(state);
    relaxed_exploration();

    int total_cost = 0;
    for (int i = 0; i < goal_propositions.size(); i++) {
        int prop_cost = goal_propositions[i]->cost;
        if (prop_cost == -1)
            return DEAD_END;
        total_cost += prop_cost;
    }
    return total_cost;
}

int AdditiveHeuristic::compute_heuristic(const State &state) {
    int h = compute_add_and_ff(state);
    if (h != DEAD_END) {
        for (int i = 0; i < goal_propositions.size(); i++)
            mark_preferred_operators(state, goal_propositions[i]);
    }
    return h;
}

ScalarEvaluator *AdditiveHeuristic::create(
    const std::vector<string> &config, int start, int &end, bool dry_run) {
    HeuristicOptions common_options;

    if (config.size() > start + 2 && config[start + 1] == "(") {
        end = start + 2;
        if (config[end] != ")") {
            NamedOptionParser option_parser;
            common_options.add_option_to_parser(option_parser);

            option_parser.parse_options(config, end, end, dry_run);
            end++;
        }
        if (config[end] != ")")
            throw ParseError(end);
    } else {
        end = start;
    }

    if (dry_run) {
        return 0;
    } else {
        return new AdditiveHeuristic(common_options);
    }
}
