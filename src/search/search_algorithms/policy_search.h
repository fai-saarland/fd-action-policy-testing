#pragma once

#include "../search_algorithm.h"
#include "../policy_testing/policy.h"
#include "../policy_testing/policies/remote_policy.h"
#include "../policy_testing/testing_environment.h"

namespace policy_search {
class PolicySearch : public SearchAlgorithm {
public:
    PolicySearch(const plugins::Options &opts);

    SearchStatus step();
    virtual void print_statistics() const override;

private:
    std::shared_ptr<policy_testing::Policy> policy;
    State current_state;
    policy_testing::TestingEnvironment env; // TODO: Why, what, how??

    // Plan and path, as returned by policy. Index traces current search position.
    int path_index = 0;
    StateID parent_id = StateID::no_state;
    State parent_state;
    std::vector<OperatorID> plan;
    std::vector<State> path;
};

extern void add_options_to_feature(plugins::Feature &feature);
}
