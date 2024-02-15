#include "sequence_oracle.h"

#include <memory>

#include "../engines/testing_base_engine.h"
#include "../../task_utils/successor_generator.h"
#include "../../plugins/plugin.h"
#include "../../evaluation_context.h"

namespace policy_testing {
SequenceOracle::SequenceOracle(const plugins::Options &opts)
    : Oracle(opts),
      first_oracle(opts.get<std::shared_ptr<Oracle>>("first_oracle")),
      second_oracle(opts.get<std::shared_ptr<Oracle>>("second_oracle")) {
    if (!first_oracle || !second_oracle) {
        std::cerr << "Both first and second oracle need to be provided!" << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
    }
    if (consider_intermediate_states) {
        std::cerr << "consider_intermediate_states is not supported in sequence_oracle" << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
    }
    if (report_parent_bugs) {
        std::cerr << "report_parent_bugs is not supported in sequence_oracle" << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
    }
    register_sub_component(first_oracle.get());
    register_sub_component(second_oracle.get());
}

void
SequenceOracle::initialize() {
    Oracle::initialize();
}

void SequenceOracle::set_engine(PolicyTestingBaseEngine *engine) {
    Oracle::set_engine(engine);
    first_oracle->set_engine(engine);
    second_oracle->set_engine(engine);
}

void
SequenceOracle::add_options_to_feature(plugins::Feature &feature) {
    Oracle::add_options_to_feature(feature);
    feature.add_option<std::shared_ptr<Oracle>>("first_oracle", "oracle to be invoked first");
    feature.add_option<std::shared_ptr<Oracle>>("second_oracle", "oracle to be invoked second");
}

TestResult
SequenceOracle::test(Policy &, const State &) {
    std::cerr << "SequenceOracle::test is not implemented" << std::endl;
    utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
}

TestResult
SequenceOracle::test_driver(Policy &policy, const PoolEntry &entry) {
    TestResult first_test_result = first_oracle->test_driver(policy, entry);
    if (first_test_result.bug_value > 0) {
        return first_test_result;
    }
    return second_oracle->test_driver(policy, entry);
}

class SequenceOracleFeature : public plugins::TypedFeature<Oracle, SequenceOracle> {
public:
    SequenceOracleFeature() : TypedFeature("sequence_oracle") {
        SequenceOracle::add_options_to_feature(*this);
    }
};
static plugins::FeaturePlugin<SequenceOracleFeature> _plugin;
} // namespace policy_testing
