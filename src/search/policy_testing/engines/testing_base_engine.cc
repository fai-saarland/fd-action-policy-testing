#include "testing_base_engine.h"

#include "../../plugins/plugin.h"
#include "../out_of_resource_exception.h"
#include "../state_regions.h"
#include "../policies/remote_policy.h"

#include <iomanip>
#include <iostream>
#include <memory>

namespace policy_testing {
PolicyTestingBaseEngine::PolicyTestingBaseEngine(const plugins::Options &opts)
    : SearchAlgorithm(opts), env_(task, &state_registry),
      policy_(opts.contains("policy") ? opts.get<std::shared_ptr<Policy>>("policy"): nullptr),
      oracle_(opts.contains("testing_method") ? opts.get<std::shared_ptr<Oracle>>("testing_method"): nullptr),
      policy_cache_file_(opts.contains("policy_cache_file") ? opts.get<std::string>("policy_cache_file") : ""),
      write_bugs_file_(opts.contains("bugs_file")),
      read_policy_cache_(opts.get<bool>("read_policy_cache")),
      just_write_policy_cache_(opts.get<bool>("just_write_policy_cache")),
      debug_(opts.get<bool>("debug")), verbose_(opts.get<bool>("verbose")) {
    testing_timer_.reset();
    testing_timer_.stop();

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
            if (!opts.get<bool>("run_without_policy")) {
                std::cerr << "You need to provide a policy." << std::endl;
                utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
            }
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

    if (write_bugs_file_) {
        bugs_stream_.open(opts.get<std::string>("bugs_file"));
    }
}

void
PolicyTestingBaseEngine::add_options_to_feature(plugins::Feature &feature, bool testing_arguments_mandatory) {
    feature.add_option<std::shared_ptr<Policy>>("policy", "", plugins::ArgumentInfo::NO_DEFAULT);
    feature.add_option<bool>("run_without_policy", "", "false");
    if (testing_arguments_mandatory) {
        feature.add_option<std::shared_ptr<Oracle>>("testing_method");
    } else {
        feature.add_option<std::shared_ptr<Oracle>>("testing_method", "", plugins::ArgumentInfo::NO_DEFAULT);
    }
    feature.add_option<std::string>("policy_cache_file", "", plugins::ArgumentInfo::NO_DEFAULT);
    feature.add_option<std::string>("bugs_file", "", plugins::ArgumentInfo::NO_DEFAULT);
    feature.add_option<bool>("read_policy_cache", "", "false");
    feature.add_option<bool>("just_write_policy_cache",
                             "Skip any calls to oracles (and thus the actual testing), just write the policy cache into the provided cache file.",
                             "false");
    feature.add_option<bool>("debug", "", "false");
    feature.add_option<bool>("verbose", "", "false");
    SearchAlgorithm::add_options_to_feature(feature);
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
PolicyTestingBaseEngine::set_max_time(timestamp_t max_time) {
    for (TestingBaseComponent *c: components_) {
        c->set_max_time(max_time);
    }
}

void
PolicyTestingBaseEngine::run_test(const PoolEntry &entry) {
    run_test(entry, static_cast<timestamp_t>(std::floor(timer->get_remaining_time())));
}

void PolicyTestingBaseEngine::print_new_bug_info(const State &state, StateID state_id) {
    std::cout << "New Bug: StateID=" << state_id << ", Values=[";
    bool first = true;
    const std::vector<int> &values = state.get_values();
    for (int val : values) {
        if (first) {
            first = false;
        } else {
            std::cout << ", ";
        }
        std::cout << val;
    }
    std::cout << "]" << std::endl;
    if (write_bugs_file_) {
        // print state
        bugs_stream_ << std::string(state_id) << "\nstate\n";
        for (int val : values) {
            bugs_stream_ << val << " ";
        }
        bugs_stream_ << std::endl;
    }
}

void
PolicyTestingBaseEngine::add_additional_bug(const State &state, TestResult test_result) {
    if (test_result.bug_value <= 0) {
        return;
    }
    const StateID state_id = state.get_id();
    auto it = bugs_.find(state_id);
    const bool bug_new = it == bugs_.end();
    const bool tested_before = non_bugs_.contains(state_id);

    if (bug_new) {
        std::cout << "Result for StateID=" << state_id << ": ";
        bugs_.emplace(state_id, test_result);
    } else {
        TestResult &stored_test_result = it->second;
        if (stored_test_result.bug_value >= test_result.bug_value) {
            // We only update the test result if a better bug value has been found
            return;
        }
        // update test result
        test_result = best_of(test_result, stored_test_result);
        stored_test_result = test_result;
        std::cout << "Result for StateID=" << state_id << ": ";
    }

    non_bugs_.erase(state_id); // does nothing if state_id is not in non_bugs_
    if (test_result.bug_value == UNSOLVED_BUG_VALUE) {
        ++num_unsolved_state_bugs_;
        std::cout << "qualitative bug found";
    } else {
        if (policy_->read_upper_policy_cost_bound(state).first != Policy::UNSOLVED) {
            std::cout << "quantitative bug found with value=" << test_result.bug_value;
        } else {
            std::cout << "unclassified bug found with value=" << test_result.bug_value;
        }
    }
    std::cout << " [t=" << utils::g_timer << "]" << std::endl;
    if (bug_new) {
        print_new_bug_info(state, state_id);
    }
    if (write_bugs_file_) {
        bugs_stream_ << std::string(state_id) << "\n" << test_result.to_string() << std::flush;
        if (bug_new && tested_before) {
            bugs_stream_ << std::string(state_id) << "\npool" << std::endl;
        }
    }
}

TestResult PolicyTestingBaseEngine::get_stored_bug_result(const State &state) {
    const StateID state_id = state.get_id();
    auto it = bugs_.find(state_id);
    if (it == bugs_.end()) {
        return {};
    } else {
        return it->second;
    }
}

void
PolicyTestingBaseEngine::run_test(const PoolEntry &entry, timestamp_t max_time) {
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
        bool bug_reported = false;
        if (!just_write_policy_cache_) {
            if (verbose_) {
                std::cout << "Running bug analysis on " << state_id << " [TestNumber=" << num_tests_ << "]..."
                          << std::endl;
            }
            TestResult test_result = oracle_->test_driver(*policy_, entry);
            std::cout << "Result for StateID=" << state_id << " [TestNumber=" << num_tests_ << "]: ";
            auto bug_it = bugs_.find(state_id);
            const bool known_bug = bug_it != bugs_.end();
            if (write_bugs_file_ && known_bug) {
                bugs_stream_ << std::string(state_id) << "\npool" << std::endl;
            }
            if (test_result.bug_value == NOT_APPLICABLE_INDICATOR) {
                std::cout << "method not applicable";
                if (!known_bug) {
                    non_bugs_.insert(state_id);
                }
            } else {
                if (test_result.bug_value == 0) {
                    std::cout << "passed";
                    if (!known_bug) {
                        non_bugs_.insert(state_id);
                    }
                } else {
                    if (known_bug) {
                        TestResult &stored_test_result = bug_it->second;
                        if (stored_test_result.bug_value != UNSOLVED_BUG_VALUE &&
                            stored_test_result.bug_value < test_result.bug_value) {
                            assert(test_result.bug_value != UNSOLVED_BUG_VALUE);
                            assert(policy_cost != Policy::UNSOLVED);
                            if (policy_cost == Policy::UNKNOWN) {
                                std::cout << "unclassified bug found with value=" << test_result.bug_value;
                            } else {
                                std::cout << "quantitative bug found with value=" << test_result.bug_value;
                            }
                            test_result = best_of(stored_test_result, test_result);
                            stored_test_result = test_result;
                            bug_reported = true;
                        } else {
                            std::cout << "bug already known, no improved bug value";
                        }
                    } else {
                        // bug is new
                        if (test_result.bug_value == UNSOLVED_BUG_VALUE) {
                            ++num_unsolved_state_bugs_;
                            std::cout << "qualitative bug found";
                        } else if (policy_cost == Policy::UNKNOWN) {
                            std::cout << "unclassified bug found with value=" << test_result.bug_value;
                        } else {
                            std::cout << "quantitative bug found with value=" << test_result.bug_value;
                        }
                        bugs_.emplace(state_id, test_result.bug_value);
                        new_bug_reported = true;
                        bug_reported = true;
                    }
                }
            }
            std::cout << " [t=" << utils::g_timer << "]" << std::endl;
            if (new_bug_reported) {
                print_new_bug_info(state, state_id);
            }
            if (write_bugs_file_) {
                if (bug_reported) {
                    bugs_stream_ << std::string(state_id) << "\n" << test_result.to_string() << std::flush;
                }
                if (new_bug_reported) {
                    bugs_stream_ << std::string(state_id) << "\npool" << std::endl;
                }
            }
        } else {
            std::cout << " [t=" << utils::g_timer << "]" << std::endl;
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
    if (oracle_ && !just_write_policy_cache_) {
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
