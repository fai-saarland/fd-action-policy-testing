#pragma once

#include "../novelty_store.h"
#include "../pool_filter.h"

#include <memory>

namespace options {
class Options;
class OptionParser;
} // namespace options

namespace policy_testing {
class NoveltyPoolFilter : public PoolFilter {
public:
    explicit NoveltyPoolFilter(const options::Options &opts);
    static void add_options_to_parser(options::OptionParser &parser);
    bool store(const State &state) override;

protected:
    void initialize() override;

private:
    const int novelty_size_;
    std::unique_ptr<NoveltyStore> novelty_;
};
} // namespace policy_testing
