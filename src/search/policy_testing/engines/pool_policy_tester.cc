#include "pool_policy_tester.h"

#include "../../option_parser.h"
#include "../../plugin.h"
#include "../out_of_resource_exception.h"

namespace policy_testing {
PoolPolicyTestingEngine::PoolPolicyTestingEngine(options::Options &opts)
    : PolicyTestingBaseEngine(opts)
      , pool_(load_pool_file(
                  task,
                  state_registry,
                  opts.get<std::string>("pool_file")))
      , novelty_store_(opts.get<int>("novelty_statistics"), task)
      , max_steps_(opts.get<int>("max_steps"))
      , first_step_(opts.get<int>("start_from"))
      , end_step_(first_step_ + max_steps_ < pool_.size() ? (first_step_ + max_steps_) : pool_.size())
      , step_(first_step_) {
    // sanity check
    {
        if (!pool_.empty()) {
            State s = state_registry.get_initial_state();
            State t = pool_[0].state;
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
    std::cout << "Pool size: " << pool_.size() << std::endl;
    utils::HashSet<StateID> pool_bugs;
    utils::HashSet<StateID> qualitative_pool_bugs;
    for (const auto &pool_entry : pool_) {
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
    for (const auto &entry : pool_) {
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
    std::cout << "Pool unconfirmed states: " << pool_.size() - pool_bugs.size() << std::endl;
    std::cout << "Non-pool bug states: " << bugs_.size() - pool_bugs.size() << std::endl;
    std::cout << "Solved pool states: " << num_solved_ << std::endl;
    novelty_store_.print_statistics();
    print_bug_statistics();
}

void
PoolPolicyTestingEngine::add_options_to_parser(options::OptionParser &parser) {
    PolicyTestingBaseEngine::add_options_to_parser(parser, true);
    parser.add_option<std::string>("pool_file");
    parser.add_option<int>("start_from", "", "0");
    parser.add_option<int>("max_steps", "", "infinity");
    parser.add_option<int>("novelty_statistics", "", "2");
}

SearchStatus
PoolPolicyTestingEngine::step() {
    if (step_ >= end_step_) {
        compute_bug_regions_print_result();
        // finish_testing();
        return FAILED;
    }

    utils::reserve_extra_memory_padding(50);

    const PoolEntry &entry = pool_[step_];
    step_++;

    novelty_store_.insert(entry.state);

    try {
        run_test(entry);
    } catch (const OutOfResourceException &) {
        utils::release_extra_memory_padding();
        std::cout.clear();
        std::cerr.clear();
        return FAILED;
    }

    utils::release_extra_memory_padding();

    return IN_PROGRESS;
}

static Plugin<SearchEngine> _plugin(
    "pool_policy_tester",
    options::parse<SearchEngine, PoolPolicyTestingEngine>);
} // namespace policy_testing
