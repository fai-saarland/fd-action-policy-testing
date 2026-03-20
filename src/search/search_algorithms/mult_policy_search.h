#pragma once

#include "../search_algorithm.h"
#include "../policy_testing/policy.h"
#include "../policy_testing/policies/remote_policy.h"
#include "../policy_testing/testing_environment.h"
#include "../heuristics/lm_cut_heuristic.h"
#include "../evaluator.h"

namespace mult_policy_search {
class MultPolicySearch : public SearchAlgorithm {
public:
    MultPolicySearch(const plugins::Options &opts);

    SearchStatus step();
    virtual void print_statistics() const override;

private:
    // Config
    bool tree_search;
    bool along_path;
    bool fault_detection;
    int steps_per_cycle;
    int number_of_policies;
    bool prefer_solved;

    std::shared_ptr<Evaluator> heuristic;

    // Logic
    std::shared_ptr<policy_testing::RemotePolicy> remote_policy;
    const State initial_state;
    State search_state; // For the "step" function of SearchAlgorithm
    policy_testing::TestingEnvironment env; // TODO: Why, what, how??

    // Plan and path, as returned by policy. Index traces current search position.
    int path_index = 0;
    StateID parent_id = StateID::no_state;
    State parent_state;

    std::vector<OperatorID> search_plan; // Plan to "conduct search" on
    std::vector<State> search_path; // Path corresponding to plan

    // Functions for plan search and plan refinement
    void run_extended_policy_search(std::vector<OperatorID> &plan, std::vector<State> &path);
    std::vector<int> imp_mult_search(State start_state, std::vector<OperatorID> &plan, std::vector<State> &path); // Returns ordered policyID by cost
    std::vector<int> imp_tree_search(State start_state, std::vector<OperatorID> &plan, std::vector<State> &path); // Returns ordered policyID by cost
    std::vector<int> imp_fault_search(std::vector<OperatorID> &plan, std::vector<State> &path); // Returns ordered policyID by cost
    void imp_along_path(std::vector<OperatorID> &plan, std::vector<State> &path, std::vector<int> policy_ids);

    //********* Miscellaneous *********/
    void append_sub_plan(int index, std::vector<OperatorID> &plan, std::vector<State> &path, std::vector<OperatorID> &sub_plan, std::vector<State> &sub_path);
    std::vector<int> accumulate_plan(std::vector<OperatorID> &plan);
    void cut_at_duplicate_state(std::vector<OperatorID> &plan, std::vector<State> &path);
    std::ostream &log_mult_pol();
};

extern void add_options_to_feature(plugins::Feature &feature);
}
