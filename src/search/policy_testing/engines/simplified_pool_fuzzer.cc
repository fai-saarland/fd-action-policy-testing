#include "simplified_pool_fuzzer.h"

#include "../../evaluation_context.h"
#include "../../evaluator.h"
#include "../../option_parser.h"
#include "../../plugin.h"
#include "../../task_utils/successor_generator.h"
#include "../../utils/countdown_timer.h"
#include "../fuzzing_bias.h"
#include "../out_of_resource_exception.h"
#include "../policy.h"
#include "../pool_filter.h"
#include "../state_regions.h"
#include "../oracle.h"
#include "../../task_utils/task_properties.h"

#include <iomanip>
#include <memory>
#include <vector>

namespace policy_testing {
SimplifiedPoolFuzzerEngine::SimplifiedPoolFuzzerEngine(const options::Options &opts)
    : PolicyTestingBaseEngine(opts)
      , novelty_store_(opts.get<bool>("disable_novelty_store") ? nullptr :
                       std::make_unique<NoveltyStore>(opts.get<int>("novelty_statistics"), task))
      , rng_(opts.get<int>("seed"))
      , eval_(opts.contains("eval") ? opts.get<std::shared_ptr<Evaluator>>("eval") : nullptr)
      , filter_(
          opts.contains("filter")
              ? opts.get<std::shared_ptr<PoolFilter>>("filter")
              : std::make_shared<PoolFilter>())
      , store_(nullptr)
      , max_steps_(opts.get<int>("max_steps"))
      , max_pool_size_(opts.get<int>("max_pool_size")) {
    fuzzing_time_.reset();
    fuzzing_time_.stop();
    if (opts.contains("pool_file")) {
        store_ = std::make_unique<PoolFile>(task, opts.get<std::string>("pool_file"));
    }
    finish_initialization({filter_.get()});
    if (debug_) {
        oracle_->print_debug_info();
    }
    report_initialized();
    fuzzing_time_.resume();
}

void
SimplifiedPoolFuzzerEngine::add_options_to_parser(options::OptionParser &parser) {
    parser.add_option<int>("max_walk_length", "", "2");

    parser.add_option<std::string>("pool_file", "", options::OptionParser::NONE);
    parser.add_option<std::shared_ptr<FuzzingBias>>("bias", "", options::OptionParser::NONE);
    parser.add_option<std::shared_ptr<PoolFilter>>("filter", "", options::OptionParser::NONE);
    parser.add_option<std::shared_ptr<Evaluator>>("eval", "", options::OptionParser::NONE);

    parser.add_option<bool>("disable_novelty_store", "", "true");
    parser.add_option<int>("novelty_statistics", "", "2");

    parser.add_option<bool>("check_policy_unsolved", "", "false");
    parser.add_option<bool>("descend_unsolved", "", "false");
    parser.add_option<int>("max_pool_size", "", "infinity");
    parser.add_option<int>("max_steps", "", "infinity");
    parser.add_option<int>("seed", "", "1734");

    PolicyTestingBaseEngine::add_options_to_parser(parser, false);
}

void
SimplifiedPoolFuzzerEngine::print_statistics() const {
    std::cout << "Fuzzing time: " << fuzzing_time_ << std::endl;
    std::cout << "Fuzzing steps: " << step_ << std::endl;
    std::cout << "Pool size: " << pool_.size() << std::endl;
    std::cout << "Max pool size: " << max_pool_size_ << std::endl;
    utils::HashSet<StateID> pool_bugs;
    utils::HashSet<StateID> qualitative_pool_bugs;
    for (const auto &pool_entry : pool_) {
        StateID pool_state = pool_entry.state.get_id();
        auto it = bugs_.find(pool_state);
        if (it != bugs_.end()) {
            pool_bugs.insert(pool_state);
            if (it->second == UNSOLVED_BUG_VALUE) {
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
    std::cout << "States filtered out: " << filtered_ << std::endl;
    std::cout << "Identified dead ends: " << dead_ends_ << std::endl;
    std::cout << "Identified goal states: " << goal_states << std::endl;
    if (novelty_store_)
        novelty_store_->print_statistics();
    filter_->print_statistics();
    print_bug_statistics();
}

void
SimplifiedPoolFuzzerEngine::print_status_line() const {
    std::cout << "Pool " << std::setw(14) << pool_.size() << " / "
              << max_pool_size_ << " ["
              << "steps=" << step_ << ", filtered=" << filtered_ << ", dead_ends=" << dead_ends_
              << ", t=" << utils::g_timer << "]" << std::endl;
}

SearchStatus
SimplifiedPoolFuzzerEngine::step() {
    utils::reserve_extra_memory_padding(50);

    bool state_inserted = false;
    if (step_ == 0 || (!frontier.empty() && step_ < max_steps_ && pool_.size() < max_pool_size_)) {
        try {
            set_max_time(timer->get_remaining_time());
            if (step_ == 0) {
                // start with initial state
                State initial_state = state_registry.get_initial_state();
                const StateID initial_state_id = initial_state.get_id();
                seen_.insert(initial_state_id);
                is_dead_.emplace(initial_state_id, false);
                state_inserted = insert(PoolEntry(-1, 0, state_registry.get_initial_state(), pool_));
            } else {
                // insert a state from the frontier
                while (!frontier.empty() && !state_inserted) {
                    auto it = rng_.choose(frontier);
                    PoolEntry entry = *it;
                    // remove entry from frontier
                    std::swap(*it, frontier.back());
                    frontier.pop_back();
                    state_inserted = insert(std::move(entry));
                }
            }
            ++step_;
        } catch (const OutOfResourceException &) {
            std::cout.clear();
            std::cerr.clear();
            std::cout << "aborting: out of time or memory [t=" << utils::g_timer << "]" << std::endl;
            utils::release_extra_memory_padding();
            fuzzing_time_.stop();
            return FAILED;
        }
    }

    if (state_inserted) {
        utils::release_extra_memory_padding();
        return IN_PROGRESS;
    } else {
        fuzzing_time_.stop();
        std::cout << "Computing state regions..." << std::endl;
        const StateRegions regions = compute_state_regions(task, state_registry, states_in_pool_);
        std::cout << "Number of regions: " << regions.size() << std::endl;
        compute_bug_regions_print_result();
        std::cout << "Simplified pool fuzzing completed." << std::endl;
        // finish_testing();
        return FAILED;
    }
}

bool
SimplifiedPoolFuzzerEngine::insert(PoolEntry &&entry) {
    // find out if insertion could take place
    const State &state = entry.state;
    if (!filter_->store(state)) {
        ++filtered_;
        return false;
    }

    // insertion is possible, compute possible successors
    std::vector<OperatorID> applicable_ops = successor_generator.generate_applicable_ops(state);
    std::vector<State> successors;
    bool is_dead_end = true;

    for (const auto &applicable_op : applicable_ops) {
        State succ = state_registry.get_successor_state(state, task_proxy.get_operators()[applicable_op]);

        // ignore state if it has been seen before and mark it as seen
        if (!seen_.insert(succ.get_id()).second) {
            assert(is_dead_.contains(succ.get_id()));
            is_dead_end &= is_dead_[succ.get_id()];
            continue;
        }

        // ignore dead end successors (if possible)
        assert(!is_dead_.contains(succ.get_id()));
        auto is_dead_insertion = is_dead_.emplace(succ.get_id(), false);
        if (eval_) {
            auto &inserted_pair = is_dead_insertion.first;
            if (is_dead_insertion.second) {
                EvaluationContext ctxt(succ);
                EvaluationResult res = eval_->compute_result(ctxt);
                if (res.is_infinite()) {
                    // mark successor as dead end
                    inserted_pair->second = true;
                    ++dead_ends_;
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
        ++dead_ends_;
        is_dead_[state.get_id()] = true;
        return false;
    }

    // state has to be inserted in pool
    states_in_pool_.insert(state.get_id());
    const int state_ref_index = static_cast<int>(pool_.size());
    pool_.push_back(entry);
    if (novelty_store_)
        novelty_store_->insert(state);
    if (store_) {
        store_->write(entry);
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
        frontier.emplace_back(state_ref_index, 1, succ, pool_);
    }
    return true;
}

bool
SimplifiedPoolFuzzerEngine::check_limits() const {
    return pool_.size() >= max_pool_size_ || timer->is_expired() || utils::is_out_of_memory();
}

static Plugin<SearchEngine>
_plugin("simplified_pool_fuzzer", options::parse<SearchEngine, SimplifiedPoolFuzzerEngine>);
} // namespace policy_testing
