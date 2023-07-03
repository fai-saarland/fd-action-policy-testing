#include "numeric_dominance_oracle.h"

#include <memory>

#include "../simulations/merge_and_shrink/abstraction_builder.h"

#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>

#include "../engines/testing_base_engine.h"
#include "../../plugin.h"


namespace policy_testing {
NumericDominanceOracle::NumericDominanceOracle(const options::Options &opts)
    : Oracle(opts),
      abstraction_builder(opts.contains("abs") ?
                          opts.get<std::shared_ptr<simulations::AbstractionBuilder>>("abs") : nullptr),
      tau_labels(std::make_shared<simulations::TauLabelManager<int>>(opts, false)),
      truncate_value(opts.get<int>("truncate_value")),
      max_simulation_time(opts.get<int>("max_simulation_time")),
      min_simulation_time(opts.get<int>("min_simulation_time")),
      max_total_time(opts.get<int>("max_total_time")),
      max_lts_size_to_compute_simulation(opts.get<int>("max_lts_size_to_compute_simulation")),
      num_labels_to_use_dominates_in(opts.get<int>("num_labels_to_use_dominates_in")),
      dump(opts.get<bool>("dump")),
      sim_file(opts.contains("sim_file") ? opts.get<std::string>("sim_file") : ""),
      write_sim_and_exit(opts.get<bool>("write_sim_and_exit")),
      test_serialization(opts.get<bool>("test_serialization")),
      local_bug_test_kind(opts.get<LocalBugTest>("local_bug_test")),
      read_simulation(opts.get<bool>("read_simulation")) {
    if ((write_sim_and_exit || read_simulation || test_serialization) && sim_file.empty()) {
        std::cerr <<
            "You need to provide (the path of) a simulation file if you want to load or write a simulation from or to disk."
                  << std::endl;
        std::exit(1);
    }
    if (!read_simulation && !abstraction_builder) {
        std::cerr << "You need to specify an abstraction builder if no simulation is to be read." << std::endl;
        std::exit(1);
    }
}

void
NumericDominanceOracle::initialize() {
    if (initialized) {
        return;
    }
    double num_dom_computation_time = -1;
    simulations::SimulationsManager::set_simulation_task(get_environment()->get_task());
    if (write_sim_and_exit || test_serialization || !read_simulation) {
        // simulation needs to be computed
        utils::Timer num_dom_timer;
        ldSim = abstraction_builder->build_abstraction(OperatorCost::NORMAL, abstractions);
        numeric_dominance_relation = ldSim->
            compute_numeric_dominance_relation<int>(truncate_value, max_simulation_time, min_simulation_time,
                                                    max_total_time, max_lts_size_to_compute_simulation,
                                                    num_labels_to_use_dominates_in,
                                                    dump, tau_labels);
        num_dom_computation_time = num_dom_timer();
        std::cout << "Computed numeric dominance function in " << num_dom_computation_time << "s" << std::endl;
        minimal_finite_dominance_value = numeric_dominance_relation->get_minimal_finite_dominance_value();
    }
    if (write_sim_and_exit) {
        {
            std::ofstream ostream(sim_file, std::ios::binary);
            {
                // make sure result is compressed using zlib_compressor
                boost::iostreams::filtering_ostreambuf filtered_stream_buf;
                filtered_stream_buf.push(boost::iostreams::zlib_compressor(boost::iostreams::zlib::best_compression));
                filtered_stream_buf.push(ostream);
                // actual serialization
                boost::archive::binary_oarchive oa{filtered_stream_buf};
                std::cout << "Writing compressed simulation file." << std::endl;
                oa << numeric_dominance_relation->strip(num_dom_computation_time);
                std::cout << "Wrote compressed simulation file." << std::endl;
            }
            // close stream in destructor
        }
        utils::exit_with(utils::ExitCode::SEARCH_UNSOLVED_INCOMPLETE);
    } else if (read_simulation) {
        std::cout << "Reading simulation file." << std::endl;
        utils::Timer read_timer;
        {
            std::ifstream istream(sim_file, std::ios::binary);
            {
                if (istream.bad() || istream.fail() || !istream.is_open()) {
                    std::cerr << "Cannot open simulation file" << std::endl;
                    std::exit(1);
                }
                // handle decompressing
                boost::iostreams::filtering_istreambuf filtering_istreambuf;
                filtering_istreambuf.push(boost::iostreams::zlib_decompressor());
                filtering_istreambuf.push(istream);
                // handle deserialization
                boost::archive::binary_iarchive ia{filtering_istreambuf};
                ia >> stripped_numeric_dominance_relation;
            }
        }
        std::cout << "Read simulation file in " << read_timer() << "s." << std::endl;
        num_dom_computation_time = stripped_numeric_dominance_relation->computation_time;
        assert(num_dom_computation_time >= 0);
        std::cout << "Computed numeric dominance function in " << num_dom_computation_time << "s" << std::endl;
        std::cout << "(Time stored in simulation file)" << std::endl;
        minimal_finite_dominance_value = stripped_numeric_dominance_relation->get_minimal_finite_dominance_value();
    } else if (test_serialization) {
        auto stripped_simulation_before = numeric_dominance_relation->strip(num_dom_computation_time);
        {
            std::ofstream ostream(sim_file, std::ios::binary);
            {
                boost::iostreams::filtering_ostreambuf filtered_stream_buf;
                filtered_stream_buf.push(boost::iostreams::zlib_compressor(boost::iostreams::zlib::best_compression));
                filtered_stream_buf.push(ostream);
                boost::archive::binary_oarchive oa{filtered_stream_buf};
                std::cout << "Writing simulation file." << std::endl;
                oa << stripped_simulation_before;
                std::cout << "Wrote simulation file." << std::endl;
            }
            // close stream in destructor
        }
        std::unique_ptr<simulations::StrippedNumericDominanceRelation> stripped_simulation_after;
        {
            std::cout << "Reading simulation file." << std::endl;
            std::ifstream istream(sim_file, std::ios::binary);
            {
                boost::iostreams::filtering_istreambuf filtering_istreambuf;
                filtering_istreambuf.push(boost::iostreams::zlib_decompressor());
                filtering_istreambuf.push(istream);
                boost::archive::binary_iarchive ia{filtering_istreambuf};
                ia >> stripped_simulation_after;
                std::cout << "Read simulation file." << std::endl;
            }
            // close stream in destructor
        }
        if (stripped_simulation_before && stripped_simulation_after &&
            *stripped_simulation_after == *stripped_simulation_before) {
            std::cout << "Serialization successful" << std::endl;
            utils::exit_with(utils::ExitCode::SEARCH_UNSOLVED_INCOMPLETE);
        } else {
            std::cerr << "Serialization failed!" << std::endl;
            utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
        }
    }
    Oracle::initialize();
}

bool NumericDominanceOracle::confirm_dominance_value(const State &dominated_state, const State &dominating_state,
                                                     int dominance_value) const {
    assert(engine_);
    if (dominance_value == simulations::MINUS_INFINITY) {
        return true;
    }
    const int dominated_cost = get_optimal_cost(dominated_state);
    if (dominated_cost == Policy::UNSOLVED) {
        return true;
    }
    assert(dominated_cost >= 0);
    const int dominating_cost = get_optimal_cost(dominating_state);
    if (dominating_cost == Policy::UNSOLVED) {
        return false;
    }
    assert(dominating_cost >= 0);
    const bool passed = (dominance_value <= dominated_cost - dominating_cost);
    if (!passed) {
        std::cerr << "Confirm dominance value failed.\nDominated state: " << dominated_state <<
            "\nCost dominated state: " << dominated_cost << "\nDominating state: " << dominating_state <<
            "\nDominating cost: " << dominating_cost << "\nClaimed dominance value: " << dominance_value << std::endl;
    }
    return passed;
}

bool NumericDominanceOracle::could_be_based_on_atomic_abstraction() {
    if (read_simulation) {
        return true;
    } else {
        return std::dynamic_pointer_cast<simulations::AbsBuilderAtomic>(abstraction_builder).get();
    }
}

BugValue
NumericDominanceOracle::local_bug_test_step(Policy &policy, const State &s, OperatorID op, const State &t,
                                            BugValue additional_bug_value) {
    assert(t == get_successor_state(s, op));
    assert(additional_bug_value >= 0);
    const int action_cost = policy.get_operator_cost(op);
    // dominance_value = D(t,s)
    const DominanceValue dominance_value = D(t, s);
    if (dominance_value > simulations::MINUS_INFINITY && action_cost > -dominance_value) {
        const BugValue local_bug_value = action_cost + dominance_value;
        assert(local_bug_value > 0);
        const BugValue combined_bug_value = local_bug_value + additional_bug_value;
        assert(combined_bug_value > 0);
        engine_->add_additional_bug(s, combined_bug_value);
#ifndef NDEBUG
        if (debug_) {
            assert(confirm_bug(s, combined_bug_value));
        }
#endif
        return combined_bug_value;
    } else {
        if (additional_bug_value > 0) {
            engine_->add_additional_bug(s, additional_bug_value);
#ifndef NDEBUG
            if (debug_) {
                assert(confirm_bug(s, additional_bug_value));
            }
#endif
        }
        return additional_bug_value;
    }
}

BugValue NumericDominanceOracle::complete_local_bug_test(Policy &policy, const State &start) {
    const PolicyCost upper_policy_cost_bound = policy.read_upper_policy_cost_bound(start).first;
    if (upper_policy_cost_bound == Policy::UNSOLVED) {
        return 0;
    }
    // executing the policy must yield a plan
    std::vector<OperatorID> plan;
    std::vector<State> path;
    assert(policy.has_complete_cached_path(start));
    [[maybe_unused]] const auto [result_known, solved] = policy.execute_get_plan_and_path(start, plan, path);
    assert(result_known);
    assert(solved);
    BugValue aggregated_bug_value = 0;

    assert(!path.empty());
    assert(plan.size() == path.size() - 1);
    for (unsigned int path_index = path.size() - 1; path_index > 0; --path_index) {
        const unsigned int predecessor_index = path_index - 1;
        aggregated_bug_value =
            local_bug_test_step(policy, path[predecessor_index], plan[predecessor_index], path[path_index],
                                aggregated_bug_value);
    }
    return aggregated_bug_value;
}

BugValue NumericDominanceOracle::local_bug_test_first(Policy &policy, const State &s) {
    if (policy.is_goal(s)) {
        return 0;
    }
    if (policy.read_upper_policy_cost_bound(s).first == Policy::UNSOLVED) {
        // executing the policy must yield a plan
        return 0;
    }
    assert(policy.can_lookup_action(s));
    OperatorID op = policy.lookup_action(s);
    assert(op != Policy::NO_OPERATOR);
    return local_bug_test_step(policy, s, op, get_successor_state(s, op));
}

BugValue NumericDominanceOracle::local_bug_test(Policy &policy, const State &s) {
    switch (local_bug_test_kind) {
    case LocalBugTest::NONE:
        return 0;
    case LocalBugTest::ONE:
        return local_bug_test_first(policy, s);
    case LocalBugTest::ALL:
        return complete_local_bug_test(policy, s);
    default:
        return 0;
    }
}

void
NumericDominanceOracle::add_options_to_parser(options::OptionParser &parser) {
    Oracle::add_options_to_parser(parser);
    parser.add_option<std::shared_ptr<simulations::AbstractionBuilder>>("abs", "abstraction builder",
                                                                        options::OptionParser::NONE);
    simulations::TauLabelManager<int>::add_options_to_parser(parser);
    parser.add_option<int>("truncate_value", "Assume -infinity if below minus this value", "1000");
    parser.add_option<int>("max_simulation_time",
                           "Maximum number of seconds spent in computing a single update of a simulation", "1800");
    parser.add_option<int>("min_simulation_time",
                           "Minimum number of seconds spent in computing a single update of a simulation", "1");
    parser.add_option<int>("max_total_time",
                           "Maximum number of seconds spent in computing all updates of a simulation", "1800");
    parser.add_option<int>("max_lts_size_to_compute_simulation",
                           "Avoid computing simulation on ltss that have more states than this number",
                           "1000000");
    parser.add_option<int>("num_labels_to_use_dominates_in",
                           "Use dominates_in for instances that have less than this amount of labels",
                           "0");
    parser.add_option<bool>("dump", "Dumps the relation that has been found", "false");
    parser.add_enum_option<LocalBugTest>(
        "local_bug_test", local_bug_test_strings,
        "Apply local bug test not at all (NONE), only for the state it is called for (ONE) or for all states in "
        "the path induced by executing the policy on the state (ALL)",
        "ALL");
    parser.add_option<std::string>("sim_file",
                                   "The file to write a computed simulation to or to read a simulation from.",
                                   options::OptionParser::NONE);
    parser.add_option<bool>("write_sim_and_exit",
                            "Only compute the specified dominance function, write it to the sim_file and exit.",
                            "false");
    parser.add_option<bool>("read_simulation", "Read simulation from sim_file instead of computing it.", "false");
    parser.add_option<bool>("test_serialization", "Write simulation to disk, read it and make sure it coincides.",
                            "false");
}

TestResult NumericDominanceOracle::test(Policy &, const State &) {
    std::cerr << "No test method for the base class NumericDominanceOracle is implemented. Use a derived oracle."
              << std::endl;
    utils::exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
}

static Plugin<Oracle>
_plugin("dummy_oracle", options::parse<Oracle, NumericDominanceOracle>);
} // namespace policy_testing
