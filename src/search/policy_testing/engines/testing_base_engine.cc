#include "testing_base_engine.h"

#include "../../plugins/plugin.h"
#include "../policies/remote_policy.h"
#include "../utils/custom_exceptions.h"
#include "../utils/state_regions.h"

#include <iomanip>
#include <iostream>
#include <memory>

namespace policy_testing {
PolicyTestingBaseEngine::PolicyTestingBaseEngine(const plugins::Options &opts)
    : SearchAlgorithm(opts), env(task, &state_registry),
      policy(opts.contains("policy") ? opts.get<std::shared_ptr<Policy>>("policy"): nullptr),
      oracle(opts.contains("oracle") ? opts.get<std::shared_ptr<Oracle>>("oracle"): nullptr),
      policy_cache_file(opts.contains("policy_cache_file") ? opts.get<std::string>("policy_cache_file") : ""),
      write_bugs_file(opts.contains("bugs_file")),
      read_policy_cache(opts.get<bool>("read_policy_cache")),
      just_write_policy_cache(opts.get<bool>("just_write_policy_cache")),
      debug(opts.get<bool>("debug")), verbose(opts.get<bool>("verbose")),
      abstain_if_first_state_not_known_solved(opts.get<bool>("abstain_if_first_state_not_known_solved")),
      print_bug_states(opts.get<bool>("print_bug_states")) {
    testing_timer.reset();
    testing_timer.stop();

    if (read_policy_cache || just_write_policy_cache) {
        if (!opts.contains("policy_cache_file")) {
            std::cerr << "You need to provide a policy cache file if you plan to write to or read from it" << std::endl;
            utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
        }
    }

    if (!policy) {
        if (RemotePolicy::connection_established()) {
            utils::g_log << "No additional policy specification found. "
                "Assuming global remote_policy with standard configuration." << std::endl;
            policy = RemotePolicy::get_global_default_policy();
        } else {
            if (!opts.get<bool>("run_without_policy")) {
                std::cerr << "You need to provide a policy." << std::endl;
                utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
            }
        }
    }

    if (read_policy_cache && just_write_policy_cache) {
        std::cerr << "You cannot read and write to the policy cache in the same run." << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
    }

    if (policy) {
        engine_components.insert(policy.get());
    } else {
        utils::g_log << "no policy provided" << std::endl;
    }

    if (oracle) {
        oracle->set_engine(this);
        engine_components.insert(oracle.get());
    } else {
        utils::g_log << "no oracle provided" << std::endl;
    }

    if (write_bugs_file) {
        bugs_stream.open(opts.get<std::string>("bugs_file"));
    }
}

void
PolicyTestingBaseEngine::add_options_to_feature(plugins::Feature &feature, bool testing_arguments_mandatory) {
    feature.add_option<std::shared_ptr<Policy>>("policy", "The policy to test (optional). Also consider using global a global remote policy.", plugins::ArgumentInfo::NO_DEFAULT);
    feature.add_option<bool>("run_without_policy", "Run engine without policy.", "false");
    if (testing_arguments_mandatory) {
        feature.add_option<std::shared_ptr<Oracle>>("oracle", "The oracle to be used.");
    } else {
        feature.add_option<std::shared_ptr<Oracle>>("oracle", "The oracle to be used.", plugins::ArgumentInfo::NO_DEFAULT);
    }
    feature.add_option<std::string>("policy_cache_file", "Policy cache file to write to or read from.", plugins::ArgumentInfo::NO_DEFAULT);
    feature.add_option<std::string>("bugs_file", "Path to bugs file.", plugins::ArgumentInfo::NO_DEFAULT);
    feature.add_option<bool>("read_policy_cache", "Read policy cache instead of running the policy (requires policy_cache_file)", "false");
    feature.add_option<bool>("just_write_policy_cache",
                             "Skip any calls to oracles (and thus the actual testing), just write the policy cache into the provided cache file.",
                             "false");
    feature.add_option<bool>("debug", "Run in (very expensive) debug mode.", "false");
    feature.add_option<bool>("verbose", "More verbose output for debugging.", "false");
    feature.add_option<bool>("abstain_if_first_state_not_known_solved",
                             "Abort the testing if the first tested state is not solved (possibly within the provided step limit)",
                             "false");
    feature.add_option<bool>("print_bug_states",
                             "Print out all found bug states including state values",
                             "false");
    SearchAlgorithm::add_options_to_feature(feature);
}

void
PolicyTestingBaseEngine::finish_initialization(std::initializer_list<TestingBaseComponent *> add_components) {
    for (TestingBaseComponent *c: add_components) {
        if (c) {
            engine_components.insert(c);
        }
    }
    for (TestingBaseComponent *c: engine_components) {
        c->connect_environment(&env);
    }
    if (read_policy_cache) {
        policy->read_running_policy_cache(policy_cache_file);
    }
    if (just_write_policy_cache) {
        policy->set_running_cache_writer(policy_cache_file);
    }
}

void
PolicyTestingBaseEngine::set_max_time(timestamp_t max_time) {
    for (TestingBaseComponent *c: engine_components) {
        c->set_max_time(max_time);
    }
}

void
PolicyTestingBaseEngine::run_test(const PoolEntry &entry) {
    run_test(entry, static_cast<timestamp_t>(std::floor(timer->get_remaining_time())));
}

void PolicyTestingBaseEngine::print_new_bug_info(const State &state, StateID state_id) {
    const std::vector<int> &values = state.get_values();
    if (print_bug_states) {
        std::cout << "New Bug: StateID=" << state_id << ", Values=[";
        bool first = true;
        for (int val : values) {
            if (first) {
                first = false;
            } else {
                std::cout << ", ";
            }
            std::cout << val;
        }
        std::cout << "]" << std::endl;
    }
    if (write_bugs_file) {
        // print state
        bugs_stream << std::string(state_id) << "\nstate\n";
        for (int val : values) {
            bugs_stream << val << " ";
        }
        bugs_stream << std::endl;
    }
}

void
PolicyTestingBaseEngine::add_additional_bug(const State &state, TestResult test_result) {
    if (test_result.bug_value <= 0) {
        return;
    }
    const StateID state_id = state.get_id();
    auto it = bugs.find(state_id);
    const bool bug_new = it == bugs.end();
    const bool tested_before = non_bugs.contains(state_id);

    if (bug_new) {
        std::cout << "Result for StateID=" << state_id << ": ";
        bugs.emplace(state_id, test_result);
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

    non_bugs.erase(state_id); // does nothing if state_id is not in non_bugs_
    if (test_result.bug_value == UNSOLVED_BUG_VALUE) {
        ++num_unsolved_state_bugs;
        std::cout << "qualitative bug found";
    } else {
        if (policy->read_upper_policy_cost_bound(state).first != Policy::UNSOLVED) {
            std::cout << "quantitative bug found with value=" << test_result.bug_value;
        } else {
            std::cout << "unclassified bug found with value=" << test_result.bug_value;
        }
    }
    std::cout << " [t=" << utils::g_timer << "]" << std::endl;
    if (bug_new) {
        print_new_bug_info(state, state_id);
    }
    if (write_bugs_file) {
        bugs_stream << std::string(state_id) << "\n" << test_result.to_string() << std::flush;
        if (bug_new && tested_before) {
            bugs_stream << std::string(state_id) << "\npool" << std::endl;
        }
    }
}

TestResult PolicyTestingBaseEngine::get_stored_bug_result(const State &state) {
    const StateID state_id = state.get_id();
    auto it = bugs.find(state_id);
    if (it == bugs.end()) {
        return {};
    } else {
        return it->second;
    }
}

void
PolicyTestingBaseEngine::run_test(const PoolEntry &entry, timestamp_t max_time) {
    if (!oracle && !just_write_policy_cache) {
        return;
    }
    const State &state = entry.state;
    const StateID state_id = state.get_id();
    testing_timer.resume();
    set_max_time(max_time);
    const bool is_first_test = num_tests == 0;
    ++num_tests;
    std::cout << "Starting test " << std::setw(5) << num_tests
              << " [t=" << utils::g_timer << "]" << std::endl;
    if (debug) {
        std::cout << "(Debug) StateID=" << state_id << ": " << state << std::endl;
    }
    try {
        if (verbose) {
            std::cout << "Executing policy on StateID=" << state_id
                      << " [TestNumber=" << num_tests << "]..." << std::endl;
        }
        const PolicyCost policy_cost = policy->compute_policy_cost(state);
        std::cout << "Policy on StateID=" << state_id << " [TestNumber=" << num_tests << "]:  ";
        if (policy_cost == Policy::UNKNOWN) {
            std::cout << "aborted [t=" << utils::g_timer << "]" << std::endl;
        } else if (policy_cost == Policy::UNSOLVED) {
            std::cout << "not solved [t=" << utils::g_timer << "]" << std::endl;
        } else {
            assert(policy_cost >= 0);
            std::cout << "policy_cost=" << policy_cost << " [t=" << utils::g_timer << "]" << std::endl;
            ++num_solved;
        }

        if (is_first_test && abstain_if_first_state_not_known_solved && policy_cost < 0) {
            // first tested state is not solved, abstain from testing problem
            std::cout << "First tested state is not (known to be) solved by the policy.\n"
                "Abstaining from problem." << std::endl;
            throw AbstentionException();
        }

        if (debug && policy_cost >= 0) {
            std::vector<OperatorID> plan;
            policy->execute_get_plan(state, plan, 0);
            std::cout << "(Debug) plan:\n";
            for (auto &i: plan) {
                std::cout << "(Debug)  " << task_proxy.get_operators()[i].get_name() << "\n";
            }
            std::cout << std::flush;
        }
        bool new_bug_reported = false;
        bool bug_reported = false;
        if (!just_write_policy_cache) {
            if (verbose) {
                std::cout << "Running bug analysis on " << state_id << " [TestNumber=" << num_tests << "]..."
                          << std::endl;
            }
            TestResult test_result = oracle->test_driver(*policy, entry);
            std::cout << "Result for StateID=" << state_id << " [TestNumber=" << num_tests << "]: ";
            auto bug_it = bugs.find(state_id);
            const bool known_bug = bug_it != bugs.end();
            if (write_bugs_file && known_bug) {
                bugs_stream << std::string(state_id) << "\npool" << std::endl;
            }
            if (test_result.bug_value == NOT_APPLICABLE_INDICATOR) {
                std::cout << "method not applicable";
                if (!known_bug) {
                    non_bugs.insert(state_id);
                }
            } else {
                if (test_result.bug_value == 0) {
                    std::cout << "passed";
                    if (!known_bug) {
                        non_bugs.insert(state_id);
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
                            ++num_unsolved_state_bugs;
                            std::cout << "qualitative bug found";
                        } else if (policy_cost == Policy::UNKNOWN) {
                            std::cout << "unclassified bug found with value=" << test_result.bug_value;
                        } else {
                            std::cout << "quantitative bug found with value=" << test_result.bug_value;
                        }
                        bugs.emplace(state_id, test_result.bug_value);
                        new_bug_reported = true;
                        bug_reported = true;
                    }
                }
            }
            std::cout << " [t=" << utils::g_timer << "]" << std::endl;
            if (new_bug_reported) {
                print_new_bug_info(state, state_id);
            }
            if (write_bugs_file) {
                if (bug_reported) {
                    bugs_stream << std::string(state_id) << "\n" << test_result.to_string() << std::flush;
                }
                if (new_bug_reported) {
                    bugs_stream << std::string(state_id) << "\npool" << std::endl;
                }
            }
        } else {
            std::cout << " [t=" << utils::g_timer << "]" << std::endl;
        }
        std::cout << std::endl;
        testing_timer.stop();
    } catch (const OutOfResourceException &) {
        std::cout.clear();
        std::cerr.clear();
        std::cout << "out of time!" << " [t=" << utils::g_timer << "]" << std::endl;
        testing_timer.stop();
        throw;
    }
}

void
PolicyTestingBaseEngine::compute_bug_regions_print_result() {
    if (oracle && !just_write_policy_cache) {
        std::cout << "Computing bug regions..." << std::endl;

        const StateRegions regions = compute_state_regions(task, state_registry, bugs);
        std::cout << "Number of bug regions: " << regions.size() << std::endl;
    }
}

void
PolicyTestingBaseEngine::print_bug_statistics() const {
    if (oracle && !just_write_policy_cache) {
        std::cout << "Testing time: " << testing_timer << std::endl;
        std::cout << "Conducted tests: " << num_tests << std::endl;
        std::cout << "Unclear states: " << non_bugs.size() << std::endl;
        std::cout << "Bugs found: " << bugs.size() << std::endl;
        std::cout << "Unsolved state bugs: " << num_unsolved_state_bugs
                  << std::endl;
        std::cout << "States solved by policy: " << num_solved << std::endl;
        oracle->print_statistics();
    }
}
} // namespace policy_testing
