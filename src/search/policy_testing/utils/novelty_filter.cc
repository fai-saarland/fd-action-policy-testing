#include "novelty_filter.h"

#include <memory>

#include "../../plugins/plugin.h"

namespace policy_testing {
NoveltyPoolFilter::NoveltyPoolFilter(const plugins::Options &opts)
    : novelty_size(opts.get<int>("novelty")) {
}

void
NoveltyPoolFilter::initialize() {
    if (initialized) {
        return;
    }
    novelty_store = std::make_unique<NoveltyStore>(novelty_size, get_task());
    PoolFilter::initialize();
}

void
NoveltyPoolFilter::add_options_to_feature(plugins::Feature &feature) {
    feature.add_option<int>("novelty", "Max arity.");
}

bool
NoveltyPoolFilter::store(const State &state) {
    return novelty_store->insert(state);
}

class NoveltyPoolFilterFeature : public plugins::TypedFeature<PoolFilter, NoveltyPoolFilter> {
public:
    NoveltyPoolFilterFeature() : TypedFeature("novelty_filter") {
        NoveltyPoolFilter::add_options_to_feature(*this);
    }
};
static plugins::FeaturePlugin<NoveltyPoolFilterFeature> _plugin;
} // namespace policy_testing
