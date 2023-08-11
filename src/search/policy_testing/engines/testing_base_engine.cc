#include "testing_base_engine.h"

#include "../../option_parser.h"
#include "../../utils/countdown_timer.h"
#include "../component.h"
#include "../out_of_resource_exception.h"
#include "../policy.h"
#include "../pool.h"
#include "../state_regions.h"
#include "../oracle.h"
#include "../policies/remote_policy.h"

#include <iomanip>
#include <iostream>
#include <memory>

namespace policy_testing {
PolicyTestingBaseEngine::PolicyTestingBaseEngine(const options::Options &opts)
    : SearchEngine(opts), env_(task, &state_registry),
      policy_(opts.contains("policy") ? opts.get<std::shared_ptr<Policy>>("policy"): nullptr),
      oracle_(opts.contains("testing_method") ? opts.get<std::shared_ptr<Oracle>>("testing_method"): nullptr),
      bugs_store_(nullptr),
      policy_cache_file_(opts.contains("policy_cache_file") ? opts.get<std::string>("policy_cache_file") : ""),
      read_policy_cache_(opts.get<bool>("read_policy_cache")),
      just_write_policy_cache_(opts.get<bool>("just_write_policy_cache")),
      debug_(opts.get<bool>("debug")), verbose_(opts.get<bool>("verbose")) {
    testing_timer_.reset();
    testing_timer_.stop();
    if (oracle_ != nullptr && opts.contains("bugs_file")) {
        bugs_store_ = std::make_unique<BugStoreFile>(task, opts.get<std::string>("bugs_file"));
    }

    if (read_policy_cache_ || just_write_policy_cache_) {
        if (!opts.contains("policy_cache_file")) {
            std::cerr << "You need to provide a policy cache file if you plan to write to or read from it" << std::endl;
            utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
        }
    }

    if (!policy_ && !read_policy_cache_) {
        if (RemotePolicy::connection_established()) {
            utils::g_log << "No additional policy specification found. "
                "Assuming global remote_policy with standard configuration." << std::endl;
            policy_ = RemotePolicy::get_global_default_policy();
        } else {
            std::cerr << "You need to provide a policy." << std::endl;
            utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
        }
    }

    if (read_policy_cache_ && just_write_policy_cache_) {
        std::cerr << "You cannot read and write to the policy cache in the same run." << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
    }

    if (policy_) {
        components_.insert(policy_.get());
    }

    if (oracle_) {
        oracle_->set_engine(this);
        components_.insert(oracle_.get());
    }
}

void
PolicyTestingBaseEngine::add_options_to_parser(
    options::OptionParser &parser,
    bool testing_arguments_mandatory) {
    parser.add_option<std::shared_ptr<Policy>>("policy", "", options::OptionParser::NONE);
    if (testing_arguments_mandatory) {
        parser.add_option<std::shared_ptr<Oracle>>("testing_method");
    } else {
        parser.add_option<std::shared_ptr<Oracle>>("testing_method", "", options::OptionParser::NONE);
    }
    parser.add_option<std::string>(
        "bugs_file", "", options::OptionParser::NONE);
    parser.add_option<std::string>(
        "policy_cache_file", "", options::OptionParser::NONE);
    parser.add_option<bool>("read_policy_cache", "", "false");
    parser.add_option<bool>("just_write_policy_cache",
                            "Skip any calls to oracles (and thus the actual testing), just write the policy cache into the provided cache file.",
                            "false");
    parser.add_option<bool>("debug", "", "false");
    parser.add_option<bool>("verbose", "", "false");
    SearchEngine::add_options_to_parser(parser);
}

void
PolicyTestingBaseEngine::finish_initialization(
    std::initializer_list<TestingBaseComponent *> components) {
    for (TestingBaseComponent *c: components) {
        if (c) {
            components_.insert(c);
        }
    }
    for (TestingBaseComponent *c: components_) {
        c->connect_environment(&env_);
    }
    if (read_policy_cache_) {
        policy_->read_running_policy_cache(policy_cache_file_);
    }
    if (just_write_policy_cache_) {
        policy_->set_running_cache_writer(policy_cache_file_);
    }
}

void
PolicyTestingBaseEngine::set_max_time(int max_time) {
    for (TestingBaseComponent *c: components_) {
        c->set_max_time(max_time);
    }
}

void
PolicyTestingBaseEngine::run_test(const PoolEntry &entry) {
    run_test(entry, std::floor(timer->get_remaining_time()));
}

void PolicyTestingBaseEngine::print_new_bug_info(const State &state, StateID state_id) {
    std::cout << "New Bug: StateID=" << state_id << ", Values=[";
    state.unpack();
    bool first = true;
    for (int val : state.get_unpacked_values()) {
        if (first) {
            first = false;
        } else {
            std::cout << ", ";
        }
        std::cout << val;
    }
    std::cout << "]" << std::endl;
}

void
PolicyTestingBaseEngine::add_additional_bug(const State &state, BugValue bug_value) {
    if (bug_value <= 0) {
        return;
    }
    const StateID state_id = state.get_id();

    bool new_bug_reported = false;
    auto it = bugs_.find(state_id);

    if (it == bugs_.end()) {
        // bug is new
        std::cout << "Result for StateID=" << state_id << ": ";
        bugs_.emplace(state_id, bug_value);
        new_bug_reported = true;
    } else {
        // bug is already known
        BugValue &stored_bug_value = it->second;
        if (stored_bug_value >= bug_value) {
            // No reason to adapt bug value
            return;
        }
        // update bug value
        stored_bug_value = bug_value;
        std::cout << "Result for StateID=" << state_id << ": ";
    }

    non_bugs_.erase(state_id); // does nothing if state_id is not in non_bugs_
    if (bug_value == UNSOLVED_BUG_VALUE) {
        ++num_unsolved_state_bugs_;
        std::cout << "qualitative bug found";
    } else {
        if (policy_->read_upper_policy_cost_bound(state).first != Policy::UNSOLVED) {
            std::cout << "quantitative bug found with value=" << bug_value;
        } else {
            std::cout << "unclassified bug found with value=" << bug_value;
        }
    }
    if (bugs_store_) {
        bugs_store_->write(state, bug_value);
    }
    std::cout << " [t=" << utils::g_timer << "]" << std::endl;
    if (new_bug_reported) {
        print_new_bug_info(state, state_id);
    }
}

BugValue PolicyTestingBaseEngine::get_stored_bug_value(const State &state) {
    const StateID state_id = state.get_id();
    auto it = bugs_.find(state_id);
    if (it == bugs_.end()) {
        return 0;
    } else {
        return it->second;
    }
}

void
PolicyTestingBaseEngine::run_test(const PoolEntry &entry, int max_time) {
    if (!oracle_ && !just_write_policy_cache_) {
        return;
    }
    const State &state = entry.state;
    const StateID state_id = state.get_id();
    testing_timer_.resume();
    set_max_time(max_time);
    ++num_tests_;
    std::cout << "Starting test " << std::setw(5) << num_tests_ << " [t=" << utils::g_timer << "]" << std::endl;
    if (debug_) {
        std::cout << "(Debug) StateID=" << state_id << ": " << state << std::endl;
    }
    try {
        if (verbose_) {
            std::cout << "Executing policy on StateID=" << state_id
                      << " [TestNumber=" << num_tests_ << "]..." << std::endl;
        }
        const PolicyCost policy_cost = policy_->compute_policy_cost(state);
        std::cout << "Policy on StateID=" << state_id << " [TestNumber=" << num_tests_ << "]:  ";
        if (policy_cost == Policy::UNKNOWN) {
            std::cout << "aborted [t=" << utils::g_timer << "]" << std::endl;
        } else if (policy_cost == Policy::UNSOLVED) {
            std::cout << "not solved [t=" << utils::g_timer << "]" << std::endl;
        } else {
            assert(policy_cost >= 0);
            std::cout << "policy_cost=" << policy_cost << " [t=" << utils::g_timer << "]" << std::endl;
            ++num_solved_;
        }

        if (debug_ && policy_cost >= 0) {
            std::vector<OperatorID> plan;
            policy_->execute_get_plan(state, plan, 0);
            std::cout << "(Debug) plan:\n";
            for (auto &i: plan) {
                std::cout << "(Debug)  " << task_proxy.get_operators()[i].get_name() << "\n";
            }
            std::cout << std::flush;
        }
        bool new_bug_reported = false;
        if (!just_write_policy_cache_) {
            if (verbose_) {
                std::cout << "Running bug analysis on " << state_id << " [TestNumber=" << num_tests_ << "]..."
                          << std::endl;
            }
            const BugValue bug_value = oracle_->test_driver(*policy_, entry);
            std::cout << "Result for StateID=" << state_id << " [TestNumber=" << num_tests_ << "]: ";
            if (bug_value == NOT_APPLICABLE_INDICATOR) {
                std::cout << "method not applicable";
                if (bugs_.find(state_id) == bugs_.end()) {
                    non_bugs_.insert(state_id);
                }
            } else {
                auto bug_it = bugs_.find(state_id);
                const bool known_bug = bug_it != bugs_.end();
                if (bug_value == 0) {
                    std::cout << "passed";
                    if (!known_bug) {
                        non_bugs_.insert(state_id);
                    }
                } else {
                    if (known_bug) {
                        int &stored_bug_value = bug_it->second;
                        if (stored_bug_value != UNSOLVED_BUG_VALUE && stored_bug_value < bug_value) {
                            assert(bug_value != UNSOLVED_BUG_VALUE);
                            assert(policy_cost != Policy::UNSOLVED);
                            if (policy_cost == Policy::UNKNOWN) {
                                std::cout << "unclassified bug found with value=" << bug_value;
                            } else {
                                std::cout << "quantitative bug found with value=" << bug_value;
                            }
                            stored_bug_value = bug_value;
                            if (bugs_store_) {
                                bugs_store_->write(entry.state, bug_value);
                            }
                        } else {
                            std::cout << "bug already known, no improved bug value";
                        }
                    } else {
                        // bug is new
                        if (bug_value == UNSOLVED_BUG_VALUE) {
                            ++num_unsolved_state_bugs_;
                            std::cout << "qualitative bug found";
                        } else if (policy_cost == Policy::UNKNOWN) {
                            std::cout << "unclassified bug found with value=" << bug_value;
                        } else {
                            std::cout << "quantitative bug found with value=" << bug_value;
                        }
                        bugs_.emplace(state_id, bug_value);
                        if (bugs_store_) {
                            bugs_store_->write(entry.state, bug_value);
                        }
                        new_bug_reported = true;
                    }
                }
            }
        }
        std::cout << " [t=" << utils::g_timer << "]" << std::endl;
        if (!just_write_policy_cache_) {
            if (new_bug_reported) {
                print_new_bug_info(state, state_id);
            }
        }
        std::cout << std::endl;
        testing_timer_.stop();
    } catch (const OutOfResourceException &) {
        std::cout.clear();
        std::cerr.clear();
        std::cout << "out of time!" << " [t=" << utils::g_timer << "]" << std::endl;
        testing_timer_.stop();
        throw;
    }
}

void
PolicyTestingBaseEngine::compute_bug_regions_print_result() {
    if (oracle_ && !just_write_policy_cache_) {
        std::cout << "Computing bug regions..." << std::endl;
        const StateRegions regions = compute_state_regions(task, state_registry, bugs_);
        std::cout << "Number of bug regions: " << regions.size() << std::endl;
    }
}

void
PolicyTestingBaseEngine::print_bug_statistics() const {
    if (oracle_ && !just_write_policy_cache_ /*&& !test_policy_cache_*/) {
        std::cout << "Testing time: " << testing_timer_ << std::endl;
        std::cout << "Conducted tests: " << num_tests_ << std::endl;
        std::cout << "Unclear states: " << non_bugs_.size() << std::endl;
        std::cout << "Bugs found: " << bugs_.size() << std::endl;
        std::cout << "Unsolved state bugs: " << num_unsolved_state_bugs_ << std::endl;
        std::cout << "States solved by policy: " << num_solved_ << std::endl;
        oracle_->print_statistics();
    }
}
} // namespace policy_testing
