#pragma once

#include "../../search_engine.h"
#include "../policies/asnets.h"

namespace policy_testing {
class ASNetBenchmarkingEngine : public SearchEngine {
public:
    explicit ASNetBenchmarkingEngine(const options::Options &opts);
    static void add_options_to_parser(options::OptionParser &parser);
    void print_statistics() const override { }

protected:
    SearchStatus step() override;

    policy_testing::ASNetInterface policy_;
};
} // namespace policy_testing
