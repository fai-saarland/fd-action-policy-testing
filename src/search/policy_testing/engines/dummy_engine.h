#pragma once

#include "../pool.h"
#include "testing_base_engine.h"

namespace policy_testing {
class DummyEngine : public PolicyTestingBaseEngine {
public:
    explicit DummyEngine(options::Options &opts);
    static void add_options_to_parser(options::OptionParser &parser);

protected:
    SearchStatus step() override;
};
} // namespace policy_testing
