#include "cost_estimator.h"

#include "../plugin.h"

namespace policy_testing {
static PluginTypePlugin<PlanCostEstimator> _plugin_type("plan_cost_estimator", "");
} // namespace policy_testing
