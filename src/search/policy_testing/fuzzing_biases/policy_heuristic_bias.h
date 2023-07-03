#include "../fuzzing_bias.h"
#include "../../option_parser.h"
#include "../../plan_manager.h"
#include "../../evaluator.h"
#include "../policy.h"
#include "../oracle.h"
#include "../cost_estimators/internal_planner_cost_estimator.h"

namespace policy_testing {
class policyHeuristicBias : public PolicyBasedBias {
public:
    explicit policyHeuristicBias(const options::Options &opts);
    static void add_options_to_parser(options::OptionParser &parser);
    int bias(const State &state, unsigned int budget) override;
    bool can_exclude_state(const State &s) override;

private:
    std::shared_ptr<Evaluator> h;
};
} //namespace policy_testing
