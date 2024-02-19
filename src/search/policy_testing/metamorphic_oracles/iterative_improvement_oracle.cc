#include "iterative_improvement_oracle.h"

#include <memory>
#include <queue>

#include "../simulations/merge_and_shrink/abstraction_builder.h"
#include "../engines/testing_base_engine.h"
#include "../../task_utils/successor_generator.h"


#include "../../heuristic.h"
#include "../../evaluation_context.h"

namespace policy_testing {
IterativeImprovementOracle::IterativeImprovementOracle(const plugins::Options &opts)
    : NumericDominanceOracle(opts),
      upper_cost_bounds(Policy::UNSOLVED),
      max_state_comparisons(static_cast<unsigned int>(std::max(opts.get<int>("max_state_comparisons"), 0))),
      conduct_lookahead_search(opts.get<bool>("conduct_lookahead_search")),
      update_parents(opts.get<bool>("update_parents")),
      max_lookahead_state_comparisons(static_cast<unsigned int>(std::max(opts.get<int>("max_lookahead_state_comparisons"), 0))),
      lookahead_heuristic(opts.get<std::shared_ptr<Evaluator>>("lookahead_heuristic", nullptr)),
      deferred_evaluation(opts.get<bool>("deferred_evaluation")),
      lookahead_comp(opts.get<LookaheadComp>("lookahead_comp")),
      max_lookahead_state_visits(static_cast<unsigned int>(std::max(opts.get<int>("max_lookahead_state_visits"), 0))),
      domain_unit_cost_and_invertible(opts.get<bool>("domain_unit_cost_and_invertible")) {
    if (consider_intermediate_states && !update_parents) {
        std::cerr << "update_parents cannot be disabled if consider_intermediate_states is enabled." << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
    }
}

void
IterativeImprovementOracle::initialize() {
    NumericDominanceOracle::initialize();
}

void
IterativeImprovementOracle::add_options_to_feature(plugins::Feature &feature) {
    NumericDominanceOracle::add_options_to_feature(feature);
    feature.add_option<int>("max_state_comparisons", "Maximal number of states to compare bug candidates to",
                            "1000000");
    feature.add_option<int>("max_lookahead_state_comparisons",
                            "Maximal number of states to compare bug candidates to withing lookahead search",
                            "1000000");
    feature.add_option<bool>("conduct_lookahead_search", "Enables lookahead search", "true");
    feature.add_option<bool>("update_parents", "Pass cost bounds to policy parent states", "true");
    feature.add_option<std::shared_ptr<Evaluator>>("lookahead_heuristic", "Heuristic to be used in lookahead search.",
                                                   plugins::ArgumentInfo::NO_DEFAULT);
    feature.add_option<bool>("deferred_evaluation",
                             "Defer heuristic evaluation in lookahead_search.\n"
                             "Not implemented in qual_lookahead_search yet.",
                             "false");
    feature.add_option<bool>("domain_unit_cost_and_invertible",
                             "Performs optimizations assuming that the task is unit cost and the domain is invertible.",
                             "false");
    feature.add_option<int>("max_lookahead_state_visits",
                            "Maximal number of states visited in lookahead search",
                            "100");
    feature.add_option<LookaheadComp>("lookahead_comp",
                                      "The comparator to be used in lookahead search; h (resembles GBFS) or g+h (resembles A*)",
                                      "h");
}

BugValue IterativeImprovementOracle::test_impl(Policy &policy, const State &state, bool local_test, bool lookahead) {
    // skip if state is already known to be a bug
    if (!tested_states.insert(state.get_id()).second) {
        const BugValue stored_bug_value = engine_->get_stored_bug_result(state).bug_value;
        if (stored_bug_value > 0) {
            return stored_bug_value;
        }
        // remove it to guarantee that the cost sets remain consistent
        // it is then added later (just like if a new state would be added)
        removeState(state, upper_cost_bounds[state]);
    }

    const auto [lower_policy_cost_bound_new_state,
                policy_bound_is_exact] = policy.read_lower_policy_cost_bound(state);
    const PolicyCost upper_policy_cost_bound_new_state =
        policy_bound_is_exact ? lower_policy_cost_bound_new_state : Policy::UNSOLVED;

    PolicyCost improved_cost_new_state = Policy::min_cost(upper_policy_cost_bound_new_state, upper_cost_bounds[state]);

    BugValue bug_value = 0;
    if (local_test) {
        bug_value = local_bug_test(policy, state);
        if (bug_value > 0 && bug_value < UNSOLVED_BUG_VALUE && policy_bound_is_exact) {
            PolicyCost inferred_cost_bound = upper_policy_cost_bound_new_state - bug_value;
            improved_cost_new_state = Policy::min_cost(improved_cost_new_state, inferred_cost_bound);
        }
    }

    unsigned int compared_states = 0;

    for (const CostSetRef &set_ref : CostSetIterator(upper_policy_cost_bound_new_state, set_refs)) {
        const auto &cost_set = getCostSet(set_ref);
        const int original_cost_old_state = set_ref.cost;
        for (const State &old_state : cost_set) {
            ++compared_states;
            if (original_cost_old_state != Policy::UNSOLVED) {
                // attempt to obtain better policy cost values for both state and old_state via comparisons
                // dominance_old_new = D(old_state, state)
                // dominating state is in first position in get_dominance_value
                const int dominance_old_new = D(old_state, state);
#ifndef NDEBUG
                if (debug_) {
                    assert(confirm_dominance_value(old_state, state, dominance_old_new));
                }
#endif
                if (dominance_old_new > simulations::MINUS_INFINITY) {
                    assert(original_cost_old_state >= 0);
                    const int inferred_cost = original_cost_old_state - dominance_old_new;
                    assert(inferred_cost >= 0);
                    improved_cost_new_state = Policy::min_cost(improved_cost_new_state, inferred_cost);
                    assert(improved_cost_new_state >= 0);
                }
            }

            if (improved_cost_new_state != Policy::UNSOLVED) {
                // dominance_new_old = D(state, old_state)
                // dominating state is in 1st position in get_dominance_value
                const int dominance_new_old = D(state, old_state);
#ifndef NDEBUG
                if (debug_) {
                    assert(confirm_dominance_value(state, old_state, dominance_new_old));
                }
#endif
                PolicyCost improved_cost_old_state = original_cost_old_state;
                if (dominance_new_old > simulations::MINUS_INFINITY) {
                    assert(improved_cost_new_state >= 0);
                    const int inferred_cost = improved_cost_new_state - dominance_new_old;
                    assert(inferred_cost >= 0);
                    improved_cost_old_state = Policy::min_cost(improved_cost_old_state, inferred_cost);
                    assert(improved_cost_old_state >= 0);
                }
                // report old state as a bug if possible
                if (original_cost_old_state != improved_cost_old_state) {
                    assert(improved_cost_old_state >= 0);
                    update_cost(old_state, original_cost_old_state, improved_cost_old_state);
                    const PolicyCost lower_policy_cost_bound_old_state = policy.read_lower_policy_cost_bound(
                        old_state).first;
                    if (Policy::is_less(improved_cost_old_state, lower_policy_cost_bound_old_state)) {
                        const BugValue old_state_bug_value =
                            (lower_policy_cost_bound_old_state == Policy::UNSOLVED) ? UNSOLVED_BUG_VALUE :
                            (lower_policy_cost_bound_old_state - improved_cost_old_state);
                        assert(engine_);
                        assert(old_state_bug_value > 0);
                        engine_->add_additional_bug(old_state,
                                                    TestResult(old_state_bug_value, improved_cost_old_state));
#ifndef NDEBUG
                        if (debug_) {
                            assert(confirm_bug(old_state, old_state_bug_value));
                        }
#endif
                    }
                }
            }

            // abort comparing with old states if max_state_comparisons is reached
            if (compared_states >= max_state_comparisons) {
                goto comparisons_finished;
            }
        }
    }
 comparisons_finished:
    assert(compared_states == max_state_comparisons || compared_states == cost_set_size);

    // remember new state
    upper_cost_bounds[state] = improved_cost_new_state;
    addState(state, improved_cost_new_state);

    // make sure upper_cost_bounds are again consistent with state sets and update their parent states
    reorder_state_sets_with_parent_updates(policy);

    // potentially conduct lookahead search, which updates the cost bound on its own
    if (lookahead &&
        ((upper_policy_cost_bound_new_state == improved_cost_new_state && policy_bound_is_exact) ||
         !policy_bound_is_exact)) {
        improved_cost_new_state =
            Policy::min_cost(improved_cost_new_state, lookahead_search(policy, state, max_lookahead_state_visits));
    }

    // and report bug value
    if (Policy::is_less(improved_cost_new_state, lower_policy_cost_bound_new_state)) {
        assert(improved_cost_new_state >= 0);
        bug_value = std::max(bug_value,
                             (lower_policy_cost_bound_new_state == Policy::UNSOLVED) ? UNSOLVED_BUG_VALUE :
                             (lower_policy_cost_bound_new_state - improved_cost_new_state));
#ifndef NDEBUG
        if (debug_) {
            assert(confirm_bug(state, bug_value));
        }
#endif
        return bug_value;
    } else {
        return 0;
    }
}

TestResult
IterativeImprovementOracle::test(Policy &, const State &) {
    std::cerr << "IterativeImprovementOracle::test is not implemented" << std::endl;
    utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
}


TestResult
IterativeImprovementOracle::test_driver(Policy &policy, const PoolEntry &entry) {
    const State &new_state = entry.state;
    BugValue bug_value = 0;

    // execute policy on new_state; needs to be done once -> do not replace with read_lower_policy_cost_bound(new_state)
    const PolicyCost lower_policy_cost_bound = policy.compute_lower_policy_cost_bound(new_state).first;

    // preprocessing
    PolicyCost preprocessing_cost_bound = Policy::UNSOLVED;
    if (domain_unit_cost_and_invertible) {
        // make sure unsolved states are reported as bugs
        if (lower_policy_cost_bound == Policy::UNSOLVED) {
            bug_value = UNSOLVED_BUG_VALUE;
        }
        // attempt to get cost bound
        if (entry.ref_state == StateID::no_state) {
            goto main_test;
        }
        const State &ref_state = get_state_registry().lookup_state(entry.ref_state);
        const PolicyCost ref_cost_bound =
            Policy::min_cost(upper_cost_bounds[ref_state], policy.read_upper_policy_cost_bound(ref_state).first);
        if (ref_cost_bound == Policy::UNSOLVED) {
            if (bug_value > 0) {
                report_parents_as_bugs(policy, new_state, TestResult(bug_value));
            }
            goto main_test;
        }
        preprocessing_cost_bound = ref_cost_bound + entry.steps;
        // report solved states as bugs if possible
        assert(preprocessing_cost_bound != Policy::UNSOLVED);
        if (lower_policy_cost_bound != Policy::UNSOLVED && preprocessing_cost_bound < lower_policy_cost_bound) {
            bug_value = lower_policy_cost_bound - preprocessing_cost_bound;
        }
        if (bug_value > 0) {
            report_parents_as_bugs(policy, new_state, TestResult(bug_value, preprocessing_cost_bound));
        }
    }

 main_test:
    if (consider_intermediate_states && bug_value <= 0) {
        std::vector<State> path = policy.execute_get_path_fragment(new_state);
        assert(!path.empty());
        // call test for intermediate states (in reverse order)
        for (auto it = path.crbegin(); it != std::prev(path.crend()); ++it) {
            const State &intermediate_state = *it;
            const BugValue intermediate_bug_value = test_impl(policy, intermediate_state, false, false);
            if (intermediate_bug_value > 0) {
                engine_->add_additional_bug(intermediate_state,
                                            TestResult(intermediate_bug_value, upper_cost_bounds[intermediate_state]));
            }
            update_parent_cost(policy, intermediate_state);
            reorder_state_sets();
        }
    }
    bug_value = std::max(bug_value, test_impl(policy, new_state, true, conduct_lookahead_search && bug_value <= 0));
    if (bug_value > 0 && update_parents) {
        update_parent_cost(policy, new_state);
        reorder_state_sets();
    }
    if (bug_value > 0 && preprocessing_cost_bound != Policy::UNSOLVED) {
        add_external_cost_bound(policy, new_state, preprocessing_cost_bound);
    }
    return TestResult(bug_value, upper_cost_bounds[new_state]);
}

void
IterativeImprovementOracle::update_cost(const State &s, PolicyCost old_cost, PolicyCost new_cost) {
    const PolicyCost min_cost = Policy::min_cost(upper_cost_bounds[s], new_cost);
    delayed_cost_set_updates.emplace_back(s, old_cost, min_cost);
    upper_cost_bounds[s] = min_cost;
}

void IterativeImprovementOracle::removeState(const State &state, PolicyCost cost) {
    assert(cost_set_size);
    --cost_set_size;
    auto &cost_set = getCostSetByCost(cost);
    auto it = std::find(cost_set.begin(), cost_set.end(), state);
    assert(it != cost_set.end());
    if (it == cost_set.end()) {
        std::cerr << "Trying to remove state with id " << std::string(state.get_id())
                  << " that is not contained in cost set for cost " << cost << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
    }
    std::swap(*it, cost_set.back());
    cost_set.pop_back();
}

void IterativeImprovementOracle::reorder_state_sets() {
    for (const auto &[state, old_cost, new_cost] : delayed_cost_set_updates) {
        removeState(state, old_cost);
        addState(state, new_cost);
    }
    delayed_cost_set_updates.clear();
}

void IterativeImprovementOracle::reorder_state_sets_with_parent_updates(Policy &policy) {
    utils::HashSet<StateID> states_to_update_parents;
    if (update_parents) {
        for (const auto &[state, old_cost, new_cost] : delayed_cost_set_updates) {
            states_to_update_parents.insert(state.get_id());
        }
    }
    reorder_state_sets();
    for (const auto &state_id: states_to_update_parents) {
        update_parent_cost(policy, get_state_registry().lookup_state(state_id));
        reorder_state_sets();
    }
}

void IterativeImprovementOracle::update_parent_cost(Policy &policy, const State &s) {
    std::queue<StateID> queue;
    queue.push(s.get_id());
    utils::HashSet<StateID> processed;
    while (!queue.empty()) {
        StateID current_state = queue.front();
        queue.pop();
        if (!processed.insert(current_state).second) {
            continue;
        }
        PolicyCost current_state_cost_bound = upper_cost_bounds.read(get_state_registry(), current_state);
        if (current_state_cost_bound == Policy::UNSOLVED) {
            continue;
        }
        for (StateID parent : policy.get_policy_parent_states(current_state)) {
            const int op_cost = policy.read_action_cost(parent);
            State parent_state = get_state_registry().lookup_state(parent);
            // make sure upper bound does not exceed policy cost
            const PolicyCost old_parent_bound = upper_cost_bounds[parent_state];
            assert(current_state_cost_bound >= 0); // holds because we do not backtrack from unsolved states
            PolicyCost new_parent_bound = Policy::min_cost(old_parent_bound, current_state_cost_bound + op_cost);

            const auto [lower_policy_cost_bound_parent, policy_bound_is_exact] =
                policy.read_lower_policy_cost_bound(parent_state);
            if (policy_bound_is_exact) {
                assert(lower_policy_cost_bound_parent == policy.get_complete_policy_cost(parent_state));
                new_parent_bound = Policy::min_cost(new_parent_bound, lower_policy_cost_bound_parent);
            }
            if (Policy::is_less(new_parent_bound, lower_policy_cost_bound_parent)) {
                const BugValue parent_bug_value =
                    (lower_policy_cost_bound_parent == Policy::UNSOLVED) ? UNSOLVED_BUG_VALUE :
                    (lower_policy_cost_bound_parent - new_parent_bound);
                assert(parent_bug_value > 0);
                assert(engine_);
                engine_->add_additional_bug(parent_state, TestResult(parent_bug_value, new_parent_bound));
#ifndef NDEBUG
                if (debug_) {
                    assert(confirm_bug(parent_state, parent_bug_value));
                }
#endif
            }
            if (old_parent_bound != new_parent_bound) {
                if (tested_states.contains(parent_state.get_id())) {
                    update_cost(parent_state, old_parent_bound, new_parent_bound);
                } else {
                    upper_cost_bounds[parent_state] = new_parent_bound;
                }
                queue.push(parent);
            }
        }
    }
}

PolicyCost IterativeImprovementOracle::infer_upper_bound(Policy &policy, const State &new_state) {
    const PolicyCost old_cost_bound = upper_cost_bounds[new_state];
#ifndef NDEBUG
    if (tested_states.contains(new_state.get_id())) {
        assert(stateIsInCostSet(new_state, old_cost_bound));
    }
#endif
    PolicyCost new_cost_bound = Policy::min_cost(old_cost_bound, policy.read_upper_policy_cost_bound(new_state).first);

    unsigned int compared_states = 0;
    for (const CostSetRef &set_ref : CostSetIterator(old_cost_bound, set_refs)) {
        const auto &cost_set = getCostSet(set_ref);
        const int original_cost_old_state = set_ref.cost;
        if (original_cost_old_state == Policy::UNSOLVED) {
            continue;
        }
        for (const State &old_state : cost_set) {
            ++compared_states;
            // attempt to obtain better policy cost values for new_state
            // dominance_old_new = D(old_state, new_state), dominating state is in first position in get_dominance_value
            const int dominance_old_new = D(old_state, new_state);
#ifndef NDEBUG
            if (debug_) {
                assert(confirm_dominance_value(old_state, new_state, dominance_old_new));
            }
#endif
            assert(original_cost_old_state != Policy::UNSOLVED);
            if (dominance_old_new > simulations::MINUS_INFINITY) {
                assert(original_cost_old_state >= 0);
                const int inferred_cost = original_cost_old_state - dominance_old_new;
                assert(inferred_cost >= 0);
                new_cost_bound = Policy::min_cost(new_cost_bound, inferred_cost);
                assert(new_cost_bound >= 0);
            }

            // abort comparing with old states if max_state_comparisons is reached
            if (compared_states >= max_lookahead_state_comparisons) {
                goto comparisons_finished;
            }
        }
    }
 comparisons_finished:
    if (old_cost_bound != new_cost_bound) {
        upper_cost_bounds[new_state] = new_cost_bound;
        if (tested_states.contains(new_state.get_id())) {
            removeState(new_state, old_cost_bound);
            addState(new_state, new_cost_bound);
            const PolicyCost lower_policy_bound_new_state = policy.read_lower_policy_cost_bound(new_state).first;
            if (Policy::is_less(new_cost_bound, lower_policy_bound_new_state)) {
                const BugValue bug_value =
                    (lower_policy_bound_new_state == Policy::UNSOLVED) ? UNSOLVED_BUG_VALUE :
                    (lower_policy_bound_new_state - new_cost_bound);
                assert(bug_value > 0);
                assert(engine_);
                engine_->add_additional_bug(new_state, TestResult(bug_value, new_cost_bound));
#ifndef NDEBUG
                if (debug_) {
                    assert(confirm_bug(new_state, bug_value));
                }
#endif
            }
        }
        if (update_parents && policy.has_complete_cached_path(new_state)) {
            update_parent_cost(policy, new_state);
            reorder_state_sets();
        }
    }
    return new_cost_bound;
}

void IterativeImprovementOracle::add_external_cost_bound(
    Policy &policy, const State &new_state, PolicyCost cost_bound) {
    if (cost_bound == Policy::UNSOLVED) {
        return;
    }

    const PolicyCost old_cost_bound = upper_cost_bounds[new_state];
    const PolicyCost new_cost_bound = Policy::min_cost(old_cost_bound, cost_bound);
    if (old_cost_bound == new_cost_bound) {
        return;
    }

    if (tested_states.contains(new_state.get_id())) {
        assert(stateIsInCostSet(new_state, old_cost_bound));
        removeState(new_state, old_cost_bound);
    }

    unsigned int compared_states = 0;
    for (const CostSetRef &set_ref : CostSetIterator(old_cost_bound, set_refs)) {
        const auto &cost_set = getCostSet(set_ref);
        const int original_cost_old_state = set_ref.cost;
        for (const State &old_state : cost_set) {
            ++compared_states;
            assert(new_cost_bound != Policy::UNSOLVED);

            // dominance_new_old = D(state, old_state)
            // dominating state is in 1st position in get_dominance_value
            const int dominance_new_old = D(new_state, old_state);
#ifndef NDEBUG
            if (debug_) {
                assert(confirm_dominance_value(new_state, old_state, dominance_new_old));
            }
#endif
            PolicyCost improved_cost_old_state = original_cost_old_state;
            if (dominance_new_old > simulations::MINUS_INFINITY) {
                assert(new_cost_bound >= 0);
                const int inferred_cost = new_cost_bound - dominance_new_old;
                assert(inferred_cost >= 0);
                improved_cost_old_state = Policy::min_cost(improved_cost_old_state, inferred_cost);
                assert(improved_cost_old_state >= 0);
            }
            // report old state as a bug if possible
            if (original_cost_old_state != improved_cost_old_state) {
                assert(improved_cost_old_state >= 0);
                update_cost(old_state, original_cost_old_state, improved_cost_old_state);
                const PolicyCost lower_policy_cost_bound = policy.read_lower_policy_cost_bound(old_state).first;
                if (Policy::is_less(improved_cost_old_state, lower_policy_cost_bound)) {
                    const BugValue old_state_bug_value =
                        (lower_policy_cost_bound == Policy::UNSOLVED) ? UNSOLVED_BUG_VALUE :
                        (lower_policy_cost_bound - improved_cost_old_state);
                    assert(engine_);
                    assert(old_state_bug_value > 0);
                    engine_->add_additional_bug(old_state, TestResult(old_state_bug_value, improved_cost_old_state));
#ifndef NDEBUG
                    if (debug_) {
                        assert(confirm_bug(old_state, old_state_bug_value));
                    }
#endif
                }
            }

            // abort comparing with old states if max_state_comparisons is reached
            if (compared_states >= max_state_comparisons) {
                goto comparisons_finished;
            }
        }
    }

 comparisons_finished:
    upper_cost_bounds[new_state] = new_cost_bound;
    addState(new_state, new_cost_bound);
    reorder_state_sets_with_parent_updates(policy);

    if (update_parents) {
        update_parent_cost(policy, new_state);
        reorder_state_sets();
    }
}

PolicyCost IterativeImprovementOracle::lookahead_search(Policy &policy, const State &s, unsigned int max_state_visits) {
    struct search_node {
        StateID state;
        int g_value;
        int h_value;
        search_node(StateID state, int g_value, int h_value) : state(state), g_value(g_value), h_value(h_value) {}
    };

    StateRegistry &registry = get_state_registry();
    const TaskProxy &proxy = get_task_proxy();

    auto comp = [&](const search_node &a, const search_node &b) {
            switch (lookahead_comp) {
            case LookaheadComp::H:
                return a.h_value > b.h_value;
            case LookaheadComp::G_PLUS_H:
                return a.h_value + a.g_value > b.h_value + b.g_value;
            default:
                assert(0);
                return false;
            }
        };
    std::priority_queue<search_node, std::vector<search_node>, decltype(comp)> queue(comp);
    utils::HashSet<StateID> visited;
    const StateID start_state_id = s.get_id();

    queue.emplace(start_state_id, 0, 0); // h value does not matter for start state

    PolicyCost upper_bound_for_start = upper_cost_bounds[s];
    bool handling_start_state = true;

    for (unsigned int i = 0; i < max_state_visits && !queue.empty();) {
        auto[current_state_id, current_g, stored_h] = queue.top();
        queue.pop();
        if (!visited.insert(current_state_id).second) {
            continue;
        }
        ++i;
        const State &current_state = registry.lookup_state(current_state_id);

        if (!handling_start_state) {
            if (task_properties::is_goal_state(get_task_proxy(), current_state)) {
                upper_bound_for_start = Policy::min_cost(upper_bound_for_start, current_g);
            }
            const PolicyCost new_bound = Policy::add_cost(current_g, infer_upper_bound(policy, current_state));
            upper_bound_for_start = Policy::min_cost(upper_bound_for_start, new_bound);
        } else {
            handling_start_state = false;
        }

        int curr_h = stored_h;
        if (lookahead_heuristic && deferred_evaluation) {
            EvaluationContext context(current_state);
            EvaluationResult res = lookahead_heuristic->compute_result(context);
            if (res.is_infinite()) {
                continue;
            }
            curr_h = res.get_evaluator_value();
        }

        std::vector<OperatorID> applicable_ops;
        generate_applicable_ops(current_state, applicable_ops);

        for (const auto &applicable_op : applicable_ops) {
            State succ = registry.get_successor_state(current_state, proxy.get_operators()[applicable_op]);
            const PolicyCost succ_g = policy.get_operator_cost(applicable_op) + current_g;
            EvaluationContext context(succ);
            int h_value = 0;
            if (lookahead_heuristic) {
                if (deferred_evaluation) {
                    h_value = curr_h;
                } else {
                    EvaluationResult succ_h = lookahead_heuristic->compute_result(context);
                    if (succ_h.is_infinite()) {
                        continue;
                    }
                    h_value = succ_h.get_evaluator_value();
                }
            }
            queue.emplace(succ.get_id(), succ_g, h_value);
        }
    }

    PolicyCost &cost_bound = upper_cost_bounds[s];
    const PolicyCost bound_before_update = cost_bound;
    if (bound_before_update != upper_bound_for_start) {
        cost_bound = upper_bound_for_start;
        if (tested_states.contains(s.get_id())) {
            removeState(s, bound_before_update);
            addState(s, upper_bound_for_start);
        }
        if (update_parents) {
            update_parent_cost(policy, s);
            reorder_state_sets();
        }
    }
    return upper_bound_for_start;
}

class IterativeImprovementOracleFeature : public plugins::TypedFeature<Oracle,
                                                                       IterativeImprovementOracle> {
public:
    IterativeImprovementOracleFeature() : TypedFeature("iterative_improvement_oracle") {
        IterativeImprovementOracle::add_options_to_feature(*this);
    }
};
static plugins::FeaturePlugin<IterativeImprovementOracleFeature> _plugin;


static plugins::TypedEnumPlugin<IterativeImprovementOracle::LookaheadComp> _enum_plugin({
        {"h", "heuristic value only (resembles GBFS)."},
        {"g_plus_h", "f=g+h (resembles A*)"}});
} // namespace policy_testing
