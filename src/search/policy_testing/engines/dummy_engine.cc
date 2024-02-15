#include "dummy_engine.h"

#include "../../plugins/plugin.h"
#include <memory>

namespace policy_testing {
DummyEngine::DummyEngine(const plugins::Options &opts)
    : PolicyTestingBaseEngine(opts) {
    finish_initialization({});
    report_initialized();
    if (oracle_) {
        oracle_->print_debug_info();
    }
    std::exit(0);
}

void
DummyEngine::add_options_to_feature(plugins::Feature &feature) {
    PolicyTestingBaseEngine::add_options_to_feature(feature, false);
}

SearchStatus
DummyEngine::step() {
    return SearchStatus::FAILED;
}

class DummyEngineFeature : public plugins::TypedFeature<SearchAlgorithm, DummyEngine> {
public:
    DummyEngineFeature() : TypedFeature("dummy_engine") {
        DummyEngine::add_options_to_feature(*this);
    }
};
static plugins::FeaturePlugin<DummyEngineFeature> _plugin;
} // namespace policy_testing
