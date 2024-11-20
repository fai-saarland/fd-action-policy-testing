#include "simplified_pool_fuzzer.h"

#include "../../evaluation_context.h"
#include "../../evaluator.h"
#include "../../plugins/plugin.h"
#include "../../task_utils/successor_generator.h"
#include "../../task_utils/task_properties.h"
#include "../utils/custom_exceptions.h"
#include "../utils/pool_filter.h"
#include "../utils/state_regions.h"

#include <iomanip>
#include <memory>
#include <vector>

namespace policy_testing {
SimplifiedPoolFuzzerEngine::SimplifiedPoolFuzzerEngine(const plugins::Options &opts)
    : PolicyTestingBaseEngine(opts),
      novelty_store(opts.get<bool>("disable_novelty_store") ? nullptr : std::make_unique<NoveltyStore>(opts.get<int>("novelty_statistics"), task)),
      rng(opts.get<int>("seed")),
      eval(opts.contains("eval") ? opts.get<std::shared_ptr<Evaluator>>("eval") : nullptr),
      filter(opts.contains("filter") ? opts.get<std::shared_ptr<PoolFilter>>("filter") : std::make_shared<PoolFilter>()),
      pool_store(nullptr),
      max_steps(opts.get<int>("max_steps")),
      max_pool_size(opts.get<int>("max_pool_size")) {
    fuzzing_time.reset();
    fuzzing_time.stop();
    if (opts.contains("pool_file")) {
        pool_store = std::make_unique<PoolFile>(task, opts.get<std::string>("pool_file"));
    }
    finish_initialization({filter.get()});
    if (debug) {
        oracle->print_debug_info();
    }
    report_initialized();
    fuzzing_time.resume();
}

void
SimplifiedPoolFuzzerEngine::add_options_to_feature(plugins::Feature &feature) {
    feature.add_option<int>("max_walk_length", "Maximal length of random walks.", "2");

    feature.add_option<std::string>("pool_file", "Path to pool file (optional).", plugins::ArgumentInfo::NO_DEFAULT);
    feature.add_option<std::shared_ptr<PoolFilter>>("filter", "Novelty filter (optional).", plugins::ArgumentInfo::NO_DEFAULT);
    feature.add_option<std::shared_ptr<Evaluator>>("eval", "Dead end evaluator (optional).", plugins::ArgumentInfo::NO_DEFAULT);

    feature.add_option<bool>("disable_novelty_store", "Disable novelty statistics.", "true");
    feature.add_option<int>("novelty_statistics", "Maximal arity in novelty statistics.", "2");

    feature.add_option<bool>("check_policy_unsolved", "Check if policy is unsolved.", "false");
    feature.add_option<bool>("descend_unsolved", "Descend if policy is unsolved.", "false");
    feature.add_option<int>("max_pool_size", "Maximal pool size.", "infinity");
    feature.add_option<int>("max_steps", "Maximal number of fuzzing steps.", "infinity");
    feature.add_option<int>("seed", "Random seed.", "1734");

    PolicyTestingBaseEngine::add_options_to_feature(feature, false);
}

void
SimplifiedPoolFuzzerEngine::print_statistics() const {
    std::cout << "Fuzzing time: " << fuzzing_time << std::endl;
    std::cout << "Fuzzing steps: " << pool_step << std::endl;
    std::cout << "Pool size: " << pool.size() << std::endl;
    std::cout << "Max pool size: " << max_pool_size << std::endl;
    utils::HashSet<StateID> pool_bugs;
    utils::HashSet<StateID> qualitative_pool_bugs;
    for (const auto &pool_entry : pool) {
        StateID pool_state = pool_entry.state.get_id();
        auto it = bugs.find(pool_state);
        if (it != bugs.end()) {
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
    std::cout << "Non-pool bug states: " << bugs.size() - pool_bugs.size() << std::endl;
    std::cout << "Solved pool states: " << num_solved << std::endl;
    std::cout << "States filtered out: " << filtered << std::endl;
    std::cout << "Identified dead ends: " << dead_ends << std::endl;
    std::cout << "Identified goal states: " << goal_states << std::endl;
    if (novelty_store) {
        novelty_store->print_statistics();
    }
    filter->print_statistics();
    print_bug_statistics();
}

void
SimplifiedPoolFuzzerEngine::print_status_line() const {
    std::cout << "Pool " << std::setw(14) << pool.size() << " / " << max_pool_size
              << " [" << "steps=" << pool_step << ", filtered=" << filtered
              << ", dead_ends=" << dead_ends << ", t=" << utils::g_timer << "]" << std::endl;
}

SearchStatus
SimplifiedPoolFuzzerEngine::step() {
    utils::reserve_extra_memory_padding(50);

    bool state_inserted = false;
    if (pool_step == 0 || (!frontier.empty() && pool_step < max_steps &&
                           pool.size() < max_pool_size)) {
        try {
            set_max_time(timer->get_remaining_time());
            if (pool_step == 0) {
                // start with initial state
                State initial_state = state_registry.get_initial_state();
                const StateID initial_state_id = initial_state.get_id();
                seen.insert(initial_state_id);
                is_dead.emplace(initial_state_id, false);
                state_inserted = insert(PoolEntry(-1, 0, state_registry.get_initial_state(), pool));
            } else {
                // insert a state from the frontier
                while (!frontier.empty() && !state_inserted) {
                    auto it = rng.choose(frontier);
                    PoolEntry entry = *it;
                    // remove entry from frontier
                    std::swap(*it, frontier.back());
                    frontier.pop_back();
                    state_inserted = insert(std::move(entry));
                }
            }
            ++pool_step;
        } catch (const OutOfResourceException &) {
            std::cout.clear();
            std::cerr.clear();
            std::cout << "aborting: out of time or memory [t=" << utils::g_timer << "]" << std::endl;
            utils::release_extra_memory_padding();
            fuzzing_time.stop();
            return FAILED;
        } catch (const AbstentionException &) {
            std::cout.clear();
            std::cerr.clear();
            std::cout << "aborting: decided to abstain from task [t=" << utils::g_timer << "]" << std::endl;
            utils::release_extra_memory_padding();
            fuzzing_time.stop();
            return FAILED;
        }
    }

    if (state_inserted) {
        utils::release_extra_memory_padding();
        return IN_PROGRESS;
    } else {
        fuzzing_time.stop();
        std::cout << "Computing state regions..." << std::endl;
        const StateRegions regions = compute_state_regions(task, state_registry, states_in_pool);
        std::cout << "Number of regions: " << regions.size() << std::endl;
        compute_bug_regions_print_result();
        std::cout << "Simplified pool fuzzing completed." << std::endl;
        return FAILED;
    }
}

bool
SimplifiedPoolFuzzerEngine::insert(PoolEntry &&entry) {
    // find out if insertion could take place
    const State &state = entry.state;
    if (!filter->store(state)) {
        ++filtered;
        return false;
    }

    // insertion is possible, compute possible successors
    std::vector<OperatorID> applicable_ops = successor_generator.generate_applicable_ops(state);
    std::vector<State> successors;
    bool is_dead_end = true;

    for (const auto &applicable_op : applicable_ops) {
        State succ = state_registry.get_successor_state(state, task_proxy.get_operators()[applicable_op]);

        // ignore state if it has been seen before and mark it as seen
        if (!seen.insert(succ.get_id()).second) {
            assert(is_dead.contains(succ.get_id()));
            is_dead_end &= is_dead[succ.get_id()];
            continue;
        }

        // ignore dead end successors (if possible)
        assert(!is_dead.contains(succ.get_id()));
        auto is_dead_insertion = is_dead.emplace(succ.get_id(), false);
        if (eval) {
            auto &inserted_pair = is_dead_insertion.first;
            if (is_dead_insertion.second) {
                EvaluationContext context(succ);
                EvaluationResult res = eval->compute_result(context);
                if (res.is_infinite()) {
                    // mark successor as dead end
                    inserted_pair->second = true;
                    ++dead_ends;
                }
            }
            if (inserted_pair->second) {
                // succ is dead end
                continue;
            } else {
                is_dead_end = false;
            }
        } else {
            is_dead_end = false;
        }
        if (check_limits()) {
            throw OutOfResourceException();
        }
        successors.push_back(succ);
    }

    // successors generated, test if state is confirmed to be a dead end
    if (is_dead_end) {
        ++dead_ends;
        is_dead[state.get_id()] = true;
        return false;
    }

    // state has to be inserted in pool
    states_in_pool.insert(state.get_id());
    const int state_ref_index = static_cast<int>(pool.size());
    pool.push_back(entry);
    if (novelty_store)
        novelty_store->insert(state);
    if (pool_store) {
        pool_store->write(entry);
    }
    print_status_line();

    // test the new pool entry if it is not a goal state
    if (task_properties::is_goal_state(task_proxy, state)) {
        ++goal_states;
    } else {
        run_test(entry);
    }

    // add successors to the frontier
    for (const State &succ : successors) {
        frontier.emplace_back(state_ref_index, 1, succ, pool);
    }
    return true;
}

bool
SimplifiedPoolFuzzerEngine::check_limits() const {
    return pool.size() >= max_pool_size || timer->is_expired() || utils::is_out_of_memory();
}

class SimplifiedPoolFuzzerFeature : public plugins::TypedFeature<SearchAlgorithm, SimplifiedPoolFuzzerEngine> {
public:
    SimplifiedPoolFuzzerFeature() : TypedFeature("simplified_pool_fuzzer") {
        SimplifiedPoolFuzzerEngine::add_options_to_feature(*this);
    }
};
static plugins::FeaturePlugin<SimplifiedPoolFuzzerFeature> _plugin;
} // namespace policy_testing
