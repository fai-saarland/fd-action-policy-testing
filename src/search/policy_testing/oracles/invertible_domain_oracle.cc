#include "invertible_domain_oracle.h"

#include "../../plugins/plugin.h"
#include "../../task_utils/task_properties.h"

#include <cassert>

namespace policy_testing {
InvertibleDomainOracle::InvertibleDomainOracle(const plugins::Options &opts)
    : Oracle(opts) {
    if (consider_intermediate_states) {
        std::cerr << "consider_intermediate_states is not supported in invertible_domain_oracle" << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
    }
    std::cout << "\n\nWARNING: Invertible domain oracle only implemented for unit cost tasks!\n"
        "Invertible domain oracle only works when initial state is solved by policy!\n" << std::endl;
}

void
InvertibleDomainOracle::add_options_to_feature(plugins::Feature &feature) {
    Oracle::add_options_to_feature(feature);
}

TestResult
InvertibleDomainOracle::test(Policy &, const State &) {
    std::cerr << "InvertibleDomainOracle::test is not implemented" << std::endl;
    utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
}

TestResult
InvertibleDomainOracle::test_driver(Policy &policy, const PoolEntry &pool_entry) {
    // TODO: extend for non unit cost tasks, make explicit that it only works when policy solves initial state
    assert(task_properties::is_unit_cost(get_task_proxy()));
    const PolicyCost lower_policy_cost_bound = policy.compute_lower_policy_cost_bound(pool_entry.state).first;
    TestResult test_result;
    if (lower_policy_cost_bound == Policy::UNSOLVED) {
        test_result = TestResult(UNSOLVED_BUG_VALUE);
        if (pool_entry.ref_state == StateID::no_state) {
            return test_result;
        }
        const PolicyCost upper_ref_cost_bound =
            policy.read_upper_policy_cost_bound(get_state_registry().lookup_state(pool_entry.ref_state)).first;
        if (upper_ref_cost_bound == Policy::UNSOLVED) {
            report_parents_as_bugs(policy, pool_entry.state, test_result);
            return test_result;
        }
        const int alt_cost = upper_ref_cost_bound + pool_entry.steps;
        test_result = TestResult(UNSOLVED_BUG_VALUE, alt_cost);
    } else {
        if (pool_entry.ref_state == StateID::no_state) {
            return TestResult(NOT_APPLICABLE_INDICATOR);
        }
        const PolicyCost upper_ref_cost_bound =
            policy.read_upper_policy_cost_bound(get_state_registry().lookup_state(pool_entry.ref_state)).first;
        if (upper_ref_cost_bound == Policy::UNSOLVED) {
            return TestResult(NOT_APPLICABLE_INDICATOR);
        }
        const int alt_cost = upper_ref_cost_bound + pool_entry.steps;
        assert(alt_cost != Policy::UNSOLVED);
        assert(lower_policy_cost_bound != Policy::UNSOLVED);
        if (alt_cost < lower_policy_cost_bound) {
            const BugValue bug_value = lower_policy_cost_bound - alt_cost;
            test_result = TestResult(bug_value, alt_cost);
        }
    }
    if (report_parent_bugs) {
        report_parents_as_bugs(policy, pool_entry.state, test_result);
    }
    return test_result;
}

class InvertibleDomainOracleFeature : public plugins::TypedFeature<Oracle, InvertibleDomainOracle> {
public:
    InvertibleDomainOracleFeature() : TypedFeature("invertible_domain_oracle") {
        InvertibleDomainOracle::add_options_to_feature(*this);
    }
};
static plugins::FeaturePlugin<InvertibleDomainOracleFeature> _plugin;
} // namespace policy_testing
