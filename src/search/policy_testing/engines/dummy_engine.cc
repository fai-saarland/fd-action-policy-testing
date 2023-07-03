#include "dummy_engine.h"

#include "../oracle.h"
#include "../../option_parser.h"
#include "../../plugin.h"

namespace policy_testing {
DummyEngine::DummyEngine(options::Options &opts)
    : PolicyTestingBaseEngine(opts) {
    finish_initialization({});
    report_initialized();
    if (oracle_) {
        oracle_->print_debug_info();
    }
    std::exit(0);
}

void
DummyEngine::add_options_to_parser(options::OptionParser &parser) {
    PolicyTestingBaseEngine::add_options_to_parser(parser, false);
}

SearchStatus
DummyEngine::step() {
    return SearchStatus::FAILED;
}

static Plugin<SearchEngine> _plugin(
    "dummy_engine",
    options::parse<SearchEngine, DummyEngine>);
} // namespace policy_testing
