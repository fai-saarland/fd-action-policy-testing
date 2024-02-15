#include "pool_filter.h"

#include "../plugins/plugin.h"

namespace policy_testing {
static class PoolFilterPlugin : public plugins::TypedCategoryPlugin<PoolFilter> {
public:
    PoolFilterPlugin() : TypedCategoryPlugin("PoolFilter") {
        document_synopsis(
            "This page describes the different PoolFilters."
            );
    }
}
_category_plugin;
}
