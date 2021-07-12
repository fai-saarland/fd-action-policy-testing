#include "heuristic_policy.h"

#include "../option_parser.h"
#include "../evaluation_context.h"
#include "../plugin.h"
#include "../policy_result.h"
#include "../search_statistics.h"
#include "../task_utils/successor_generator.h"
#include "../task_utils/task_properties.h"

using namespace std;

namespace policies {
HeuristicPolicy::HeuristicPolicy(const Options &opts)
    : Policy(opts),
      evaluator(opts.get<shared_ptr<Evaluator>>("evaluator")),
      registry(task_proxy),
      successor_generator(successor_generator::get_successor_generator(task_proxy)) {
    cout << "Initializing heuristic policy..." << endl;
}

HeuristicPolicy::~HeuristicPolicy() = default;

PolicyResult HeuristicPolicy::compute_policy(const State &state) {
    // collect all applicable actions
    vector<OperatorID> applicable_ops;
    successor_generator.generate_applicable_ops(state, applicable_ops);

    // look for action leading to the state with best (lowest) heuristic value
    int h_best = -1;
    vector<OperatorID> op_best;
    for (OperatorID op_id : applicable_ops) {
        OperatorProxy op = task_proxy.get_operators()[op_id];
        assert(task_properties::is_applicable(op, state));
        const State succ_state = registry.get_successor_state(state, op);
        EvaluationContext context = EvaluationContext(succ_state, nullptr, false);
        int h = evaluator->compute_result(context).get_evaluator_value();
        // better or first action found
        if (h_best == -1 || h < h_best) {
            h_best = h;
            op_best.clear();
        }
        if (h == h_best){
            op_best.push_back(op_id);
        }
    }

    return PolicyResult(move(op_best), vector<float>(), true);
}

bool HeuristicPolicy::dead_ends_are_reliable() const {
    return evaluator->dead_ends_are_reliable();
}

static shared_ptr<Policy> _parse(OptionParser &parser) {
    parser.document_synopsis("Heuristic Policy", "");
    parser.add_option<shared_ptr<Evaluator>>("evaluator", "evaluator");
    Policy::add_options_to_parser(parser);
    Options opts = parser.parse();

    shared_ptr<Policy> policy;
    if (!parser.dry_run()) {
        policy = make_shared<HeuristicPolicy>(opts);
    }
    return policy;
}


static Plugin<Policy> _plugin("heuristic_policy", _parse);

int LookaheadHeuristicPolicy::get_max_evaluator_value(const State &state, int depth) {
    EvaluationContext context = EvaluationContext(state, nullptr, false);
    int h_best = evaluator->compute_result(context).get_evaluator_value();

    if(depth > 0 && h_best != 0) {
        vector<OperatorID> applicable_ops;
        successor_generator.generate_applicable_ops(state, applicable_ops);
        for (OperatorID op_id : applicable_ops) {
            OperatorProxy op = task_proxy.get_operators()[op_id];
            assert(task_properties::is_applicable(op, state));
            const State successor_state = registry.get_successor_state(state, op);
            int h = get_max_evaluator_value(successor_state, depth - 1);
            if (h_best == -1 || h < h_best)
                h_best = h;
        }
    }
    return h_best;
}

PolicyResult LookaheadHeuristicPolicy::compute_policy(const State &state) {
    vector<OperatorID> applicable_ops;
    successor_generator.generate_applicable_ops(state, applicable_ops);

    int h_best = -1;
    vector<OperatorID> op_best;
    for (OperatorID op_id : applicable_ops) {
        OperatorProxy op = task_proxy.get_operators()[op_id];
        assert(task_properties::is_applicable(op, state));
        const State successor_state = registry.get_successor_state(state, op);
        int h = get_max_evaluator_value(successor_state, lookahead-1);
        // better or first action found
        if (h_best == -1 || h < h_best) {
            h_best = h;
            op_best.clear();
        }
        if (h == h_best){
            op_best.push_back(op_id);
        }
    }

    return PolicyResult(move(op_best), vector<float>(), true);
}

LookaheadHeuristicPolicy::LookaheadHeuristicPolicy(const Options &options) :
    Policy(options),
    evaluator(options.get<shared_ptr<Evaluator>>("evaluator")),
    registry(task_proxy),
    lookahead(options.get<int>("lookahead")),
    successor_generator(successor_generator::get_successor_generator(task_proxy)) {
        cout << "Initializing lookahead heuristic policy..." << endl;
}

LookaheadHeuristicPolicy::~LookaheadHeuristicPolicy() = default;

bool LookaheadHeuristicPolicy::dead_ends_are_reliable() const {
    return evaluator->dead_ends_are_reliable();
}

static shared_ptr<Policy> _parse_lookahead(OptionParser &parser) {
    parser.document_synopsis("Lookahead Heuristic Policy", "");
    parser.add_option<shared_ptr<Evaluator>>("evaluator", "evaluator");
    parser.add_option<int>("lookahead", "Lookahead used for the heuristic policy", "1");
    Policy::add_options_to_parser(parser);
    Options opts = parser.parse();

    if (!parser.dry_run())
        return make_shared<LookaheadHeuristicPolicy>(opts);

    return nullptr;
}

static Plugin<Policy> _plugin_lookahead("lookahead_heuristic_policy", _parse_lookahead);

}
