#include "cost_estimator.h"

#include "../plugins/plugin.h"

namespace policy_testing {
static class PlanCostEstimatorPlugin : public plugins::TypedCategoryPlugin<PlanCostEstimator> {
public:
    PlanCostEstimatorPlugin() : TypedCategoryPlugin("plan_cost_estimator") {
        document_synopsis(
            "This page describes the different PlanCostEstimators."
            );
    }
}
_category_plugin;
} // namespace policy_testing
