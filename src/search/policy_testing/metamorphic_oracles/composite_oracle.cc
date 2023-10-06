#include "composite_oracle.h"

#include <memory>
#include <ranges>

#include "../engines/testing_base_engine.h"
#include "../../task_utils/successor_generator.h"


#include "../../plugin.h"
#include "../../heuristic.h"
#include "../../evaluation_context.h"

namespace policy_testing {
CompositeOracle::CompositeOracle(const options::Options &opts)
    : Oracle(opts),
      qual_oracle(opts.contains("qual_oracle") ? opts.get<std::shared_ptr<Oracle>>("qual_oracle") : nullptr),
      quant_oracle(opts.contains("quant_oracle") ? opts.get<std::shared_ptr<Oracle>>("quant_oracle") : nullptr),
      metamorphic_oracle(opts.contains("metamorphic_oracle") ?
                         opts.get<std::shared_ptr<Oracle>>("metamorphic_oracle") : nullptr),
      enforce_external(opts.get<bool>("enforce_external")) {
    if (qual_oracle) {
        register_sub_component(qual_oracle.get());
    }
    if (quant_oracle) {
        register_sub_component(quant_oracle.get());
    }
    if (metamorphic_oracle) {
        register_sub_component(metamorphic_oracle.get());
    }

    if (consider_intermediate_states) {
        std::cerr << "consider_intermediate_states is not supported in composite_oracle" << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
    }
    if (report_parent_bugs) {
        std::cerr << "report_parent_bugs is not supported in composite_oracle" << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
    }
    if (metamorphic_oracle && (
            (quant_oracle && quant_oracle->report_parent_bugs) ||
            (qual_oracle && qual_oracle->report_parent_bugs))) {
        std::cerr << "report_parent_bugs should be done only via update parent cost in metamorphic oracle if the "
            "composite_oracle uses an iterative improvement oracle" << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
    }
    if (metamorphic_oracle && !metamorphic_oracle->consider_intermediate_states &&
        ((quant_oracle && quant_oracle->consider_intermediate_states) ||
         (qual_oracle && qual_oracle->consider_intermediate_states))) {
        std::cerr <<
            "if a metamorphic oracle is used and intermediate states are to be considered the also enable this "
            "in metamorphic oracle so that oracles can be combined properly" << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
    }
    if (metamorphic_oracle && std::dynamic_pointer_cast<IterativeImprovementOracle>(metamorphic_oracle) &&
        !std::dynamic_pointer_cast<IterativeImprovementOracle>(metamorphic_oracle)->update_parents) {
        std::cerr << "metamorphic oracle should be used to report parent bugs" << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
    }
}

void
CompositeOracle::initialize() {
    Oracle::initialize();
}

void CompositeOracle::set_engine(PolicyTestingBaseEngine *engine) {
    Oracle::set_engine(engine);
    if (qual_oracle) {
        qual_oracle->set_engine(engine);
    }
    if (quant_oracle) {
        quant_oracle->set_engine(engine);
    }
    if (metamorphic_oracle) {
        metamorphic_oracle->set_engine(engine);
    }
}

void
CompositeOracle::add_options_to_parser(options::OptionParser &parser) {
    Oracle::add_options_to_parser(parser);
    parser.add_option<std::shared_ptr<Oracle>>("qual_oracle", "oracle for qualitative evaluation",
                                               options::OptionParser::NONE);
    parser.add_option<std::shared_ptr<Oracle>>("quant_oracle", "oracle for quantitative evaluation",
                                               options::OptionParser::NONE);
    parser.add_option<std::shared_ptr<Oracle>>("metamorphic_oracle", "oracle for metamorphic testing",
                                               options::OptionParser::NONE);
    parser.add_option<bool>("enforce_external",
                            "run external oracle(s) on intermediate states even if pool state could be confirmed as a bug by metamorphic oracle",
                            "false");
}

TestResult
CompositeOracle::test(Policy &, const State &) {
    std::cerr << "CompositeOracle::test is not implemented" << std::endl;
    utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
}

TestResult
CompositeOracle::test_driver(Policy &policy, const PoolEntry &entry) {
    const State &state = entry.state;
    const PolicyCost upper_policy_cost_bound = policy.compute_upper_policy_cost_bound(state).first;
    if (metamorphic_oracle && metamorphic_oracle->consider_intermediate_states &&
        ((quant_oracle && quant_oracle->consider_intermediate_states) ||
         (qual_oracle && qual_oracle->consider_intermediate_states))) {
        // first run metamorphic oracle
        TestResult metamorphic_test_result = metamorphic_oracle->test_driver(policy, entry);
        if (metamorphic_test_result.bug_value > 0 && !enforce_external) {
            return metamorphic_test_result;
        }
        if (engine_->is_known_bug(state) && !enforce_external) {
            return engine_->get_stored_bug_result(state);
        }
        // run other oracle
        TestResult result;
        if (upper_policy_cost_bound != Policy::UNSOLVED) {
            if (quant_oracle) {
                if (quant_oracle->consider_intermediate_states) {
                    std::vector<State> path = policy.execute_get_path_fragment(state);
                    // call test for intermediate states (in reverse order) and for pool state
                    for (const auto &intermediate_state : std::ranges::reverse_view(path)) {
                        if (policy.is_goal(intermediate_state) || engine_->is_known_bug(intermediate_state)) {
                            continue;
                        }
                        const TestResult intermediate_test = quant_oracle->test(policy, intermediate_state);
                        if (intermediate_test.bug_value > 0) {
                            engine_->add_additional_bug(intermediate_state, intermediate_test);
                            metamorphic_oracle->add_external_cost_bound(policy, intermediate_state,
                                                                        intermediate_test.upper_cost_bound);
                            return best_of(intermediate_test, metamorphic_test_result);
                        }
                    }
                } else {
                    result = quant_oracle->test(policy, state);
                }
            }
        } else {
            if (qual_oracle) {
                if (qual_oracle->consider_intermediate_states) {
                    std::vector<State> path = policy.execute_get_path_fragment(state);
                    assert(!path.empty());
                    // call test for intermediate states (in reverse order) and for pool state
                    for (const auto &intermediate_state : std::ranges::reverse_view(path)) {
                        if (policy.is_goal(intermediate_state) || engine_->is_known_bug(intermediate_state)) {
                            continue;
                        }
                        const TestResult intermediate_test = qual_oracle->test(policy, intermediate_state);
                        if (intermediate_test.bug_value > 0) {
                            engine_->add_additional_bug(intermediate_state, intermediate_test);
                            metamorphic_oracle->add_external_cost_bound(policy, intermediate_state,
                                                                        intermediate_test.upper_cost_bound);
                            return best_of(intermediate_test, metamorphic_test_result);
                        }
                    }
                } else {
                    result = qual_oracle->test(policy, state);
                }
            }
        }
        if (result.bug_value <= 0) {
            // other oracle could not confirm bug, return test result of metamorphic oracle
            return metamorphic_test_result;
        }
        // further oracle could confirm bug, make use of improved upper bound in iterative improvement oracle
        metamorphic_oracle->add_external_cost_bound(policy, state, result.upper_cost_bound);
        return best_of(result, metamorphic_test_result);
    } else {
        if (metamorphic_oracle) {
            // first run metamorphic oracle
            TestResult metamorphic_test_result = metamorphic_oracle->test_driver(policy, entry);
            if (metamorphic_test_result.bug_value > 0) {
                return metamorphic_test_result;
            }
            if (engine_->is_known_bug(state)) {
                return engine_->get_stored_bug_result(state);
            }
            // metamorphic oracle could not confirm bug, run other oracle
            TestResult result;
            if (upper_policy_cost_bound != Policy::UNSOLVED) {
                if (quant_oracle) {
                    result = quant_oracle->test(policy, state);
                }
            } else {
                if (qual_oracle) {
                    result = qual_oracle->test(policy, state);
                }
            }
            if (result.bug_value <= 0) {
                // test could not prove bug
                return {};
            }
            // further oracle could confirm bug, make use of improved upper bound in iterative improvement oracle
            metamorphic_oracle->add_external_cost_bound(policy, state, result.upper_cost_bound);
            return result;
        } else {
            if (upper_policy_cost_bound != Policy::UNSOLVED) {
                if (quant_oracle) {
                    return quant_oracle->test_driver(policy, entry);
                }
            } else {
                if (qual_oracle) {
                    return qual_oracle->test_driver(policy, entry);
                }
            }
            return {};
        }
    }
}

static Plugin<Oracle>
_plugin("composite_oracle", options::parse<Oracle, CompositeOracle>);
} // namespace policy_testing
