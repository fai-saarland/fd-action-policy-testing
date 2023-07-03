#include "novelty_filter.h"

#include <memory>

#include "../option_parser.h"
#include "../plugin.h"

namespace policy_testing {
NoveltyPoolFilter::NoveltyPoolFilter(const options::Options &opts)
    : novelty_size_(opts.get<int>("novelty")) {
}

void
NoveltyPoolFilter::initialize() {
    if (initialized) {
        return;
    }
    novelty_ = std::make_unique<NoveltyStore>(novelty_size_, get_task());
    PoolFilter::initialize();
}

void
NoveltyPoolFilter::add_options_to_parser(options::OptionParser &parser) {
    parser.add_option<int>("novelty");
}

bool
NoveltyPoolFilter::store(const State &state) {
    return novelty_->insert(state);
}

static Plugin<PoolFilter> _plugin_filter_novelty(
    "novelty_filter",
    options::parse<PoolFilter, NoveltyPoolFilter>);
} // namespace policy_testing
