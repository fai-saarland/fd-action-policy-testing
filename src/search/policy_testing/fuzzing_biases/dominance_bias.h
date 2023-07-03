#include "../fuzzing_bias.h"
#include "../../option_parser.h"
#include "../../plan_manager.h"
#include "../../evaluator.h"
#include "../policy.h"
#include "../oracle.h"
#include "../metamorphic_oracles/numeric_dominance_oracle.h"

namespace policy_testing {
class DominanceBias : public PolicyBasedBias {
public:
    explicit DominanceBias(const options::Options &opts);
    static void add_options_to_parser(options::OptionParser &parser);
    int bias(const State &state, unsigned int budget) override;

    bool can_exclude_state(const State &) override {
        return false;
    }

private:
    const std::shared_ptr<NumericDominanceOracle> numericDominanceOracle;
};
} //namespace policy_testing
