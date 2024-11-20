#include "pool_policy_tester.h"

#include "../../plugins/plugin.h"
#include "../utils/custom_exceptions.h"

namespace policy_testing {
PoolPolicyTestingEngine::PoolPolicyTestingEngine(const plugins::Options &opts)
    : PolicyTestingBaseEngine(opts),
      pool(load_pool_file(task, state_registry, opts.get<std::string>("pool_file"))),
      novelty_store(opts.get<int>("novelty_statistics"), task),
      max_steps(opts.get<int>("max_steps")),
      first_step(opts.get<int>("start_from")),
      end_step(first_step + max_steps < pool.size() ? (first_step + max_steps) : pool.size()),
      pool_step(first_step) {
    // sanity check
    {
        if (!pool.empty()) {
            State s = state_registry.get_initial_state();
            State t = pool[0].state;
            for (unsigned var = 0; var < s.size(); ++var) {
                if (s[var].get_value() != t[var].get_value()) {
                    std::cerr << "FDR Representation does not match!" << std::endl;
                    utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
                }
            }
        }
    }
    finish_initialization({});
    report_initialized();
}

void
PoolPolicyTestingEngine::print_statistics() const {
    std::cout << "Pool size: " << pool.size() << std::endl;
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
    novelty_store.print_statistics();
    print_bug_statistics();
}

void
PoolPolicyTestingEngine::add_options_to_feature(plugins::Feature &feature) {
    PolicyTestingBaseEngine::add_options_to_feature(feature, true);
    feature.add_option<std::string>("pool_file", "The pool file to load.");
    feature.add_option<int>("start_from", "Index of first step to test.", "0");
    feature.add_option<int>("max_steps", "Number of pool states to test.", "infinity");
    feature.add_option<int>("novelty_statistics", "Maximal arity for novelty statistics.", "2");
}

SearchStatus
PoolPolicyTestingEngine::step() {
    if (pool_step >= end_step) {
        compute_bug_regions_print_result();
        return FAILED;
    }

    utils::reserve_extra_memory_padding(50);
    const PoolEntry &entry = pool[pool_step];
    pool_step++;
    novelty_store.insert(entry.state);

    try {
        run_test(entry);
    } catch (const OutOfResourceException &) {
        utils::release_extra_memory_padding();
        std::cout.clear();
        std::cerr.clear();
        return FAILED;
    } catch (const AbstentionException &) {
        std::cout.clear();
        std::cerr.clear();
        std::cout << "aborting: decided to abstain from task [t=" << utils::g_timer << "]" << std::endl;
        return FAILED;
    }

    utils::release_extra_memory_padding();
    return IN_PROGRESS;
}

class PoolPolicyTesterFeature : public plugins::TypedFeature<SearchAlgorithm, PoolPolicyTestingEngine> {
public:
    PoolPolicyTesterFeature() : TypedFeature("pool_policy_tester") {
        PoolPolicyTestingEngine::add_options_to_feature(*this);
    }
};
static plugins::FeaturePlugin<PoolPolicyTesterFeature> _plugin;
} // namespace policy_testing
