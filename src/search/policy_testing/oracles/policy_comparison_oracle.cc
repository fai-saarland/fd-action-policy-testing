#include "policy_comparison_oracle.h"

#include "../../plugins/plugin.h"
#include "../../task_utils/task_properties.h"
#include "../utils/custom_exceptions.h"
#include "../policies/remote_policy.h"
#include "../policy.h"

#include <cassert>
#include <utility>
#include <vector>
#include <optional>

namespace policy_testing {
PolicyComparisonOracle::PolicyComparisonOracle(const plugins::Options &opts)
    : Oracle(opts),
      qual_oracle(opts.contains("qual_oracle") ? opts.get<std::shared_ptr<Oracle>>("qual_oracle") : nullptr),
      maintain_upper_bounds(opts.get<bool>("maintain_upper_bounds")),
      upper_bounds(maintain_upper_bounds ? std::make_shared<UpperBoundsExtended>(30, opts.get<bool>("parent_propagation"), opts.get<bool>("register_unsolved")) : nullptr),
      majority_vote(opts.get<bool>("majority_vote")),
      num_offset_runs(opts.get<int>("num_offset_runs")),
      amount_offset(opts.get<int>("amount_offset")),
      num_per_state(majority_vote ? 0 : opts.get<int>("num_per_state")),
      random_sample(opts.get<std::string>("choose_policies") == "random"),
      online_sample(opts.get<std::string>("choose_policies") == "online_sample"),
      online_pq(opts.get<std::string>("choose_policies") == "online_pq"),
      online_handler(online_sample || online_pq ? std::make_optional<OnlineHandler>(num_per_state) : std::nullopt),
      ignore_quality_bugs(opts.get<bool>("ignore_quality_bugs")),
      num_per_path_state(opts.get<int>("num_per_path_state")),
      run_prob(opts.get<double>("run_prob")),
      rng(0) {
    if (qual_oracle) {
        register_sub_component(qual_oracle.get());
    }
    if (majority_vote && (online_sample || online_pq || num_per_state > 0 || num_per_path_state != 0)) {
        std::cerr << "Cannot have majority vote and separate policy runs." << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
    }
    if (online_sample && online_pq) {
        std::cerr << "Must decide on one online selection algorithm." << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
    }
    if (!random_sample && !online_sample && !online_pq && opts.get<std::string>("choose_policies") != "normal") {
        std::cerr << "Configuration options for 'choose_policies' are 'normal', 'random', 'online_sample', 'online_pq'." << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
    }
}

void
PolicyComparisonOracle::add_options_to_feature(plugins::Feature &feature) {
    Oracle::add_options_to_feature(feature);
    feature.add_option<bool>("ignore_quality_bugs", "If set, quality bugs are ignored.", "false");
    feature.add_option<bool>("maintain_upper_bounds", "If set, oracle will maintain and improve an upper bound u(s) >= h*(s) for each state.", "true");
    feature.add_option<bool>("majority_vote", "Whether to perform a majority vote of all policies instead of executing them as separate runs.", "false");
    feature.add_option<int>("num_offset_runs", "How many oracle runs to do with an offset (in addition to the pool state runs).", "0");
    feature.add_option<int>("amount_offset", "Random walk depth for offsetting portfolio policy execution from pool state.", "1");
    feature.add_option<int>("num_per_state", "How many portfolio policies to execute on the pool state, -1 for all available.", plugins::ArgumentInfo::NO_DEFAULT);
    feature.add_option<std::string>("choose_policies", "How to decide which policies to run. Options are 'normal', 'random', 'online_sample', 'online_pq'", "\"normal\"");
    feature.add_option<bool>("parent_propagation", "If set, UpperBounds will propagate new bounds through previous policy paths.", "true");
    feature.add_option<int>("num_per_path_state", "Number of policies run per state along the path, -1 for all available.", "0");
    feature.add_option<double>("run_prob", "Only run portfolio policies on any state with given probability.", "1.0");
    feature.add_option<std::shared_ptr<Oracle>>("qual_oracle", "Oracle to use in bugfinding, when a potential quality bug is detected.", plugins::ArgumentInfo::NO_DEFAULT);
    feature.add_option<bool>("register_unsolved", "If set, policy runs are registeren in upper bounds even if they do not reach a goal.", "false");
}

void PolicyComparisonOracle::set_engine(PolicyTestingBaseEngine *engine) {
    Oracle::set_engine(engine);
    if (qual_oracle != nullptr) {
        qual_oracle->set_engine(engine);
    }
}

auto
PolicyComparisonOracle::test_impl(RemotePolicy &remote_policy, const State &state, int num_port_exec, int offset) -> TestResult {
    std::vector<PolicyCost> policy_costs;         // Index 0 is "policy under test"
    policy_costs.push_back(execute_policy(state, remote_policy, 0).policy_result);
    if (policy_costs[0] < 0) {
        if (ignore_quality_bugs) {
            return TestResult(0, policy_costs[0]);
        }
        if (qual_oracle != nullptr) {
            return qual_oracle->test(remote_policy, state);
        }
    }

    auto [offset_state, walk_cost] = random_walk(state, offset);

    // Handle bugfinding with majority vote.
    if (majority_vote) {
        policy_costs.push_back(Policy::add_cost(execute_policy(offset_state, remote_policy, -1).bound_result, walk_cost));

        if (Policy::is_less(policy_costs[1], policy_costs[0])) {
            PolicyCost bug_value;
            if (policy_costs[0] == Policy::UNSOLVED) {
                bug_value = UNSOLVED_BUG_VALUE;
            } else {
                bug_value = policy_costs[0] - policy_costs[1];     // policy "model" < policy 0 => bug in policy 0 (under test)
            }
#ifndef NDEBUG
            if (debug) {
                assert(confirm_bug(state, bug_value));
            }
#endif
            return TestResult(bug_value, policy_costs[1]);
        }
        return TestResult(0, policy_costs[0]);
    }

    // Track executed policies to subsequently add to online priority queue.
    std::vector<int> executed_policies;
    // Keep model indices to avoid double execution with random approach.
    std::vector<int> model_indices;
    if (random_sample) {
        for (int i = 1; i < remote_policy.get_num_models(); ++i) {
            model_indices.push_back(i);
        }
        rng.shuffle(model_indices);
    }

    auto pq_copy = best_policies;
    for (int num_model = 1; (num_model <= num_port_exec || num_port_exec == -1) && num_model < policy_testing::RemotePolicy::get_num_models(); ++num_model) {
        auto [offset_state, walk_cost] = random_walk(state, offset);

        int model_idx;
        if (offset > 0) {
            if (num_model > num_offset_runs) {
                break;
            }
            if (online_sample) {
                model_idx = online_handler->take_from_best(num_model - 1, remote_policy, rng);
            } else if (online_pq) {
                model_idx = std::get<1>(pq_copy.top());
                pq_copy.pop();
            } else {
                // TODO: Missing random option here?
                model_idx = num_model;
            }
        } else if (online_sample) {
            if (rng.random() < 0.5) { // Choose from online with fixed 50% chance
                model_idx = online_handler->take_from_best(num_model - 1, remote_policy, rng);
            } else {
                model_idx = rng.random(remote_policy.get_num_models() - 1) + 1;
            }
        } else if (online_pq) {
            std::tuple<double, int> best_policy = best_policies.top();

            // auto pq = best_policies;
            // while (!pq.empty()) {
            //     auto [a, b] = pq.top();      // Structured binding (C++17+)
            //     std::cout << "(" << a << ", " << b << ")";
            //     pq.pop();
            // }
            // std::cout << std::endl;

            model_idx = std::get<1>(best_policy);
            best_policies.pop();
            executed_policies.push_back(model_idx);
        } else if (random_sample) {
            model_idx = model_indices.back();
            model_indices.pop_back();
        } else {
            model_idx = num_model;
        }

        std::cout << "Policy #" << model_idx << " run on state " << offset_state.get_id() << " [t=" << utils::g_timer << "]" << std::endl;
        policy_costs.push_back(Policy::add_cost(execute_policy(offset_state, remote_policy, model_idx, policy_costs[0]).bound_result, walk_cost));     // Max_cost: cost of policy under test
#ifndef NDEBUG
        if (debug) {
            std::cout << "Policy Cost " << model_idx << ": " << policy_costs[num_model] << std::endl;
        }
#endif
        if (Policy::is_less(policy_costs[num_model], policy_costs[0])) {
            std::cout << "Policy #" << model_idx << " found bug in state " << state.get_id() << " [t=" << utils::g_timer << "]" << std::endl;

            if ((online_sample || online_pq) && offset == 0) {
                online_handler->run_found_bug(model_idx);
                if (online_pq) {
                    for (int policy : executed_policies) {
                        best_policies.emplace((double)(online_handler->get_num_bugs(policy) + 1) / (double)online_handler->get_num_runs(policy), policy);
                    }
                    executed_policies.clear();
                }
            }

            PolicyCost bug_value;
            if (policy_costs[0] == Policy::UNSOLVED) {
                bug_value = UNSOLVED_BUG_VALUE;
            } else {
                bug_value = policy_costs[0] - policy_costs[num_model];     // policy "model" < policy 0 => bug in policy 0 (under test)
            }
#ifndef NDEBUG
            if (debug) {
                assert(confirm_bug(state, bug_value));
            }
#endif
            return TestResult(bug_value, policy_costs[num_model]);
        }
        if ((online_sample || online_pq) && offset == 0) {
            online_handler->run_no_bug(model_idx);
        }
    }
    if (online_pq && offset == 0) {
        for (int policy : executed_policies) {
            best_policies.emplace((double)(online_handler->get_num_bugs(policy) + 1) / (double)online_handler->get_num_runs(policy), policy);
        }
        executed_policies.clear();
    }
    return TestResult(0, policy_costs[0]);
}

TestResult PolicyComparisonOracle::test([[maybe_unused]] Policy &policy, [[maybe_unused]] const State &state) {
    std::cerr << "Function test not implemented in policy_comparison_oracle." << std::endl;
    utils::exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
}

auto
PolicyComparisonOracle::test_driver(Policy &policy, const PoolEntry &entry) -> TestResult {
    // Catch errors in casting and model number
    auto *remote_policy = dynamic_cast<RemotePolicy *>(&policy);
    if (remote_policy == nullptr) {
        std::cerr << "Policy Comparison oracle can only be called with a remote policy." << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
    }
    if (remote_policy->get_num_models() <= num_per_path_state || remote_policy->get_num_models() <= num_per_state) {
        std::cerr << "Cannot run " << num_per_path_state << " policies on path states, as only " << remote_policy->get_num_models() - 1 << " portfolio policies are available." << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
    }
    if (best_policies.empty()) {
        for (int i = 1; i < remote_policy->get_num_models(); ++i) {
            best_policies.push(std::make_tuple(1.0, i));
        }
    }

    // Check whether bug is known
    const State &pool_state = entry.state;
    if (engine->is_known_bug(pool_state) && !enforce_intermediate) {
        return engine->get_stored_bug_result(pool_state);
    }

    // Enter execution along path of policy under test
    if (num_per_path_state != 0 || consider_intermediate_states || enforce_intermediate) {
        std::vector<State> path;
        std::vector<OperatorID> plan;
        const auto [result_known, solved] = remote_policy->execute_get_transitions_and_path(pool_state, plan, path, 0);

        PolicyCost plan_cost;
        if (!result_known) {
            plan_cost = Policy::UNKNOWN;
        } else if (!solved) {
            plan_cost = Policy::UNSOLVED;
        } else {
            plan_cost = calculate_plan_cost(get_task(), plan);
        }

        assert(!path.empty());
        assert(!solved || path.size() == plan.size() + 1);
        assert(!result_known || solved || path.size() == plan.size());
        if (path.size() == plan.size()) {
            plan.pop_back();
        }
        assert(!result_known || path.size() == plan.size() + 1);
        assert(!plan.empty() || (plan_cost == Policy::UNSOLVED || plan_cost == Policy::UNKNOWN));
        // Call test for intermediate states (in reverse order)
        auto plan_it = plan.crbegin();
        int acc_cost = 0;
        for (auto path_it = path.crbegin(); path_it != std::prev(path.crend()); ++path_it) {
            const State &intermediate_state = *path_it;
            const OperatorID &intermediate_op = plan_it != plan.crend() ? *plan_it : Policy::NO_OPERATOR;
            if (plan_it != plan.crend()) {
                ++plan_it;
            }

            // Run on state with probability run_prob
            if (rng.random() < run_prob) {
                TestResult intermediate_result = engine->get_stored_bug_result(intermediate_state);
                if (intermediate_result.bug_value > 0) {
#ifndef NDEBUG
                    assert(confirm_bug(intermediate_state, intermediate_result.bug_value));
#endif
                    return TestResult {intermediate_result.bug_value, intermediate_result.upper_cost_bound + (plan_cost - acc_cost)};
                }

                intermediate_result = test_impl(*remote_policy, intermediate_state, num_per_path_state);
                if (intermediate_result.bug_value > 0) {
#ifndef NDEBUG
                    assert(confirm_bug(intermediate_state, intermediate_result.bug_value));
#endif
                    return TestResult {intermediate_result.bug_value, intermediate_result.upper_cost_bound + (plan_cost - acc_cost)};
                }
            }
            acc_cost += get_task_proxy().get_operators()[intermediate_op].get_cost();
        }

        if (engine->is_known_bug(pool_state)) {
            return engine->get_stored_bug_result(pool_state);
        }
    }

    // Main test on pool state
    TestResult result = test_impl(*remote_policy, pool_state, num_per_state);
    if (result.bug_value > 0) {
        return result;
    }

    // With offset
    return test_impl(*remote_policy, pool_state, num_per_state, amount_offset);
}

auto
PolicyComparisonOracle::execute_policy(const State &state, RemotePolicy &remote_policy, int model_index, int max_cost) -> BoundResult {
    if (maintain_upper_bounds) {
        return upper_bounds->run_policy(get_task_proxy(), state, remote_policy, model_index);
    } else {
        PolicyCost policy_result = remote_policy.compute_policy_cost(state, model_index, max_cost);
        return BoundResult {
            model_index,
            policy_result,
            policy_result
        };
    }
}

auto
PolicyComparisonOracle::random_walk(const State &state, int depth) -> std::pair<State, int> {
    State walk_state = state;
    int walk_cost = 0;
    for (int i = 0; i < depth; ++i) {
        std::vector<OperatorProxy> applicable_ops;
        for (OperatorProxy op : get_task_proxy().get_operators()) {
            if (task_properties::is_applicable(op, walk_state)) {
                applicable_ops.push_back(op);
            }
        }
        if (applicable_ops.size() == 0) {
            break;
        }
        OperatorProxy chosen_op = applicable_ops[rng.random(applicable_ops.size())];
        walk_state = get_state_registry().get_successor_state(walk_state, chosen_op);
        walk_cost += chosen_op.get_cost();
    }
    return std::make_pair(walk_state, walk_cost);
}

auto
PolicyComparisonOracle::set_upper_bounds(std::shared_ptr<UpperBoundsExtended> upper_bounds) -> void {
    PolicyComparisonOracle::upper_bounds = upper_bounds;
}

class PolicyComparisonOracleFeature : public plugins::TypedFeature<Oracle, PolicyComparisonOracle> {
public:
    PolicyComparisonOracleFeature() : TypedFeature("policy_comparison_oracle") {
        PolicyComparisonOracle::add_options_to_feature(*this);
    }
};
static plugins::FeaturePlugin<PolicyComparisonOracleFeature> _plugin;
} // namespace policy_testing
