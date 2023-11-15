#include "pool_fuzzer.h"

#include "../../evaluation_context.h"
#include "../../evaluator.h"
#include "../../option_parser.h"
#include "../../plugin.h"
#include "../../task_utils/successor_generator.h"
#include "../../utils/countdown_timer.h"
#include "../fuzzing_bias.h"
#include "../out_of_resource_exception.h"
#include "../pool_filter.h"
#include "../state_regions.h"
#include "../task_utils/task_properties.h"

#include <iomanip>
#include <memory>
#include <vector>

namespace policy_testing {
PoolFuzzerEngine::PoolFuzzerEngine(const options::Options &opts)
    : PolicyTestingBaseEngine(opts),
      novelty_store(opts.get<int>("novelty_statistics"), task),
      rng(opts.get<int>("seed")),
      eval(opts.contains("eval") ? opts.get<std::shared_ptr<Evaluator>>("eval") : nullptr),
      bias(opts.contains("bias") ? opts.get<std::shared_ptr<FuzzingBias>>("bias") : std::make_shared<NeutralBias>()),
      filter(
          opts.contains("filter")
              ? opts.get<std::shared_ptr<PoolFilter>>("filter")
              : std::make_shared<PoolFilter>()),
      store(nullptr),
      max_steps(opts.get<int>("max_steps")),
      max_pool_size(opts.get<int>("max_pool_size")),
      max_walk_length(opts.get<int>("max_walk_length")),
      penalize_policy_fails(opts.get<bool>("penalize_policy_fails")),
      bias_budget(opts.get<unsigned int>("bias_budget")),
      cache_bias(opts.get<bool>("cache_bias")) {
    fuzzing_time.reset();
    fuzzing_time.stop();
    if (opts.contains("pool_file")) {
        store = std::make_unique<PoolFile>(task, opts.get<std::string>("pool_file"));
    }
    finish_initialization({bias.get(), filter.get()});
    report_initialized();
    fuzzing_time.resume();
}

void
PoolFuzzerEngine::add_options_to_parser(options::OptionParser &parser) {
    parser.add_option<int>("max_walk_length", "", "5");

    parser.add_option<std::string>("pool_file", "", options::OptionParser::NONE);
    parser.add_option<std::shared_ptr<FuzzingBias>>("bias", "", options::OptionParser::NONE);
    parser.add_option<std::shared_ptr<PoolFilter>>("filter", "", options::OptionParser::NONE);
    parser.add_option<std::shared_ptr<Evaluator>>("eval", "", options::OptionParser::NONE);

    parser.add_option<int>("novelty_statistics", "", "2");
    parser.add_option<int>("max_pool_size", "", "infinity");
    parser.add_option<int>("max_steps", "", "infinity");
    parser.add_option<bool>("penalize_policy_fails",
                            "uses a bias of infinity if the policy is known to fail on the state;"
                            "only applied if policy is executed in bias computation",
                            "false");
    parser.add_option<int>("seed", "", "1734");
    parser.add_option<unsigned int>("bias_budget",
                                    "budget for bias computation in each state expansion; choose 0 to set no limit",
                                    "200");
    parser.add_option<bool>("cache_bias",
                            "indicates whether the bias should be cached for each state",
                            "false");

    PolicyTestingBaseEngine::add_options_to_parser(parser, false);
}

void
PoolFuzzerEngine::print_statistics() const {
    std::cout << "Fuzzing time: " << fuzzing_time << std::endl;
    std::cout << "Fuzzing steps: " << fuzzing_step << std::endl;
    std::cout << "Duplicate states: " << duplicates << std::endl;
    std::cout << "Pool size: " << pool.size() << std::endl;
    std::cout << "Max pool size: " << max_pool_size << std::endl;
    utils::HashSet<StateID> pool_bugs;
    utils::HashSet<StateID> qualitative_pool_bugs;
    for (const auto &pool_entry : pool) {
        StateID pool_state = pool_entry.state.get_id();
        auto it = bugs_.find(pool_state);
        if (it != bugs_.end()) {
            pool_bugs.insert(pool_state);
            if (it->second.bug_value == UNSOLVED_BUG_VALUE) {
                qualitative_pool_bugs.insert(pool_state);
            }
        }
    }
    std::cout << "Pool state ids: [";
    bool first = true;
    for (const auto &entry : pool) {
        if (first) {
            first = false;
        } else {
            std::cout << ", ";
        }
        std::cout << entry.state.get_id();
    }
    std::cout << ']' << std::endl;
    std::cout << "Pool bug states: " << pool_bugs.size() << std::endl;
    std::cout << "Qualitative pool bug states: " << qualitative_pool_bugs.size() << std::endl;
    std::cout << "Non-qualitative pool bug states: " << pool_bugs.size() - qualitative_pool_bugs.size() << std::endl;
    std::cout << "Pool unconfirmed states: " << pool.size() - pool_bugs.size() << std::endl;
    std::cout << "Non-pool bug states: " << bugs_.size() - pool_bugs.size() << std::endl;
    std::cout << "Solved pool states: " << num_solved_ << std::endl;
    std::cout << "Intermediate states added during random walks: " << intermediate_states << std::endl;
    std::cout << "States filtered out: " << filtered << std::endl;
    std::cout << "Failed attempts: " << failed << std::endl;
    novelty_store.print_statistics();
    bias->print_statistics();
    filter->print_statistics();
    print_bug_statistics();
}

void
PoolFuzzerEngine::print_status_line() const {
    std::cout << "Pool " << std::setw(14) << pool.size() << " / " << max_pool_size << " ["
              << "steps=" << fuzzing_step << ", unsolved=" << intermediate_states
              << ", filtered=" << filtered << ", t=" << utils::g_timer << "]" << std::endl;
}

SearchStatus
PoolFuzzerEngine::step() {
    if (fuzzing_step >= max_steps || pool.size() >= max_pool_size) {
        fuzzing_time.stop();

        std::cout << "Computing state regions..." << std::endl;
        const StateRegions regions = compute_state_regions(task, state_registry, states_in_pool);
        std::cout << "Number of regions: " << regions.size() << std::endl;

        compute_bug_regions_print_result();

        return FAILED;
    }

    utils::reserve_extra_memory_padding(50);
    try {
        set_max_time(timer->get_remaining_time());
        if (fuzzing_step == 0) {
            // start with initial state
            insert(-1, 0, state_registry.get_initial_state());
        } else {
            random_walk();
        }
        ++fuzzing_step;
    } catch (const OutOfResourceException &) {
        std::cout.clear();
        std::cerr.clear();
        std::cout << "aborting: out of time or memory [t=" << utils::g_timer << "]" << std::endl;
        utils::release_extra_memory_padding();
        fuzzing_time.stop();
        return FAILED;
    }
    utils::release_extra_memory_padding();

    return IN_PROGRESS;
}

bool
PoolFuzzerEngine::insert(int ref, int steps, const State &state) {
    if (!filter->store(state)) {
        ++filtered;
        return false;
    }
    states_in_pool.insert(state.get_id());
    pool.emplace_back(ref, ref < 0 ? StateID::no_state : pool[ref].state.get_id(), steps, state);
    novelty_store.insert(state);
    bias->notify_inserted(state);
    print_status_line();
    run_test(pool.back());
    // reordered run_test in store_write in order to make sure that pool only contains states for which policy has
    // been run if cached policy is used
    if (store) {
        store->write(ref, steps, state);
    }
    return true;
}

void
PoolFuzzerEngine::random_walk() {
    const int ref_index = rng(pool.size());
    const int step_limit = rng(max_walk_length) + 1;
    State state = pool[ref_index].state;
    int step_counter = 0;
    for (; step_counter < step_limit; ++step_counter) {
        std::vector<OperatorID> applicable_ops = successor_generator.generate_applicable_ops(state);
        rng.shuffle(applicable_ops); // shuffle ops as it could be that not all successors can be considered
        std::vector<State> successors;
        std::vector<int> successor_biases;
        unsigned int used_budget = 0;

        for (auto &applicable_op : applicable_ops) {
            if (check_limits()) {
                throw OutOfResourceException();
            }
            if (bias_budget && used_budget >= bias_budget) {
                // bias_budget == 0 means no budget set (-> infinity)
                break;
            }
            // calculate the remaining budget, 0 indicates no budget set (-> infinity) rather than no budget left
            assert(!bias_budget || bias_budget > used_budget);
            const unsigned int remaining_budget = bias_budget ? bias_budget - used_budget : 0;
            State succ = state_registry.get_successor_state(state, task_proxy.get_operators()[applicable_op]);
            int succ_bias = 0;
            bool read_cached_bias = false; // indicates whether a bias value for succ was cached

            if (cache_bias) {
                auto it = bias_cache.find(succ.get_id());
                if (it != bias_cache.end()) {
                    // bias is already cached
                    succ_bias = it->second;
                    read_cached_bias = true;
                    if (succ_bias == FuzzingBias::NEGATIVE_INFINITY) {
                        continue;
                    }
                }
            }
            if (!read_cached_bias) { // bias needs to be computed
                // check if succ is a goal state (in which case, we ignore it)
                if (task_properties::is_goal_state(get_task_proxy(), succ)) {
                    if (cache_bias) {
                        bias_cache.emplace(succ.get_id(), FuzzingBias::NEGATIVE_INFINITY);
                    }
                    continue;
                }
                // check dead ends
                bool succ_is_known_dead_end = false;
                if (!task_properties::exists_applicable_op(get_task_proxy(), succ)) {
                    if (cache_bias) {
                        bias_cache.emplace(succ.get_id(), FuzzingBias::NEGATIVE_INFINITY);
                    }
                    continue;
                }
                if (eval) {
                    auto insertion = is_dead.emplace(succ.get_id(), false);
                    auto &inserted_pair = insertion.first;
                    if (insertion.second) {
                        EvaluationContext ctxt(succ);
                        EvaluationResult res = eval->compute_result(ctxt);
                        inserted_pair->second = res.is_infinite();
                    }
                    succ_is_known_dead_end = inserted_pair->second;
                }
                if (succ_is_known_dead_end || bias->can_exclude_state(succ)) {
                    if (cache_bias) {
                        bias_cache.emplace(succ.get_id(), FuzzingBias::NEGATIVE_INFINITY);
                    }
                    continue;
                }
                // check policy fail and compute bias
                succ_bias = (penalize_policy_fails && bias->policy_is_known_to_fail(succ, remaining_budget)) ?
                    FuzzingBias::POSITIVE_INFINITY : bias->bias(succ, remaining_budget);
                used_budget += bias->determine_used_budget(succ, remaining_budget);
                if (cache_bias) {
                    bias_cache.emplace(succ.get_id(), succ_bias);
                }
            }
            successors.push_back(succ);
            successor_biases.push_back(succ_bias);
        }
        const State *selected_state = FuzzingBias::weighted_choose(rng, successors, successor_biases);
        if (!selected_state) {
            ++failed;
            is_dead[state.get_id()] = true;
            return;
        }
        state = *selected_state;
    }

    if (!states_in_pool.contains(state.get_id())) {
        insert(ref_index, step_counter, state);
    } else {
        ++duplicates;
    }
}

bool
PoolFuzzerEngine::check_limits() const {
    return pool.size() >= max_pool_size || timer->is_expired() || utils::is_out_of_memory();
}

static Plugin<SearchEngine>
_plugin("pool_fuzzer", options::parse<SearchEngine, PoolFuzzerEngine>);
} // namespace policy_testing
