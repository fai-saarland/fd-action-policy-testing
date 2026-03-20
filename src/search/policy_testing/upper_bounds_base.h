// #pragma once

// #include "../per_state_information.h"
// #include "policy.h"
// #include "policies/remote_policy.h"
// #include "../task_proxy.h"
// #include "engines/testing_base_engine.h"

// namespace policy_testing {
// struct BoundResult {
//     int model_idx;
//     PolicyCost policy_result;
//     PolicyCost bound_result;
// };
// /**
//  * Class for maintaining upper bounds in a state space.
//  *
//  * For propagating bounds, all previous policy paths are cached. Multiple different policies are supported.
//  * User is responsible for keeping the access-index consistent across different policies.
//  */
// class UpperBounds {
// protected:
//     // Toggle propagation through (policy) paths.
//     // If disabled, paths will not be stored in the first place.
//     bool parent_propagation = false;

//     // Upper bound on the cost of states
//     PerStateInformation<PolicyCost> upper_bounds;

//     // Map associating pool states to bug reporting function.
//     utils::HashMap<StateID, std::function<void(int)>> subsequent_bug_reporting;

//     // Caches cost for policy execution
//     PerStateInformation<std::vector<PolicyCost>> policy_cost_cache;

//     // Stores parents wrt a policy in any given state; (parent, op_cost)
//     PerStateInformation<std::vector<std::pair<State, int>>> cached_parents;

//     // Set of improved bounds: (state, old_bound, new_bound)
//     std::vector<std::tuple<State, int, int>> improved_bounds; // TODO: should this be a set or rather a vector?

//     /**
//      * Updates bound with new cost (if Policy::is_less).
//      * @returns true, if upper bound actually updated, false otherwise
//      */
//     bool update_bound(State &state, PolicyCost new_cost);

//     /**
//      * Registers a path for later bound propagation.
//      * Needs an operator sequence to determine respective added cost in propagation.
//      *
//      * @param model The policy model index for which the path should be stored.
//      * @warning path and plan must fit together, method does not check whether respective operators are actually applicable
//      */
//     void register_path(const TaskProxy &task_proxy, std::vector<OperatorID> &plan, std::vector<State> &path, int model);

// public:
//     UpperBounds(int num_policies, bool parent_propagation);

//     PolicyCost get_cost_for_state(const State &state) {
//         return upper_bounds[state];
//     }

//     /**
//      * Computes the best possible cost value for the given state.
//      * Uses cost given policy and existing cost bounds.
//      *
//      * @note Automatically updates bounds along policy path.
//      */
//     BoundResult run_policy(const TaskProxy &task_proxy, const State &state, RemotePolicy &remote_policy, int model = 0);

//     /**
//      * Calculates cost of a plan while also using UpperBounds for the bound_result field.
//      *
//      * @warning Plan must be valid!
//      * @note If no model index is explicitly specified, path will not be stored for bound propagation.
//      */
//     BoundResult run_plan(const TaskProxy &task_proxy, const State &state, std::vector<OperatorID> plan, int model = -1);

//     /**
//      * Propagates and updates all improved_bounds through paths stored by cached_parents.
//      *
//      * @param policy Only propagate for policy at index policy. Value -1 updates for all policies.
//      * @note parent_propagation must be set to true
//      */
//     void propagate_bounds(int policy = -1);

//     /**
//      * Allows to inject a new bound for some given state.
//      * Useful for external bound information e.g. by Aras or other oracles not providing information through running policies.
//      *
//      * @note Will only update bound, if new_bound is better than the existing bound
//      */
//     bool inject_upper_bound(State &state, int new_bound);

//     /**
//      * Allows to register a pool bug for subsequent bug reporting.
//      * If the bound of the pool state is improved at some point, it will be reported as a bug (if not known to be a bug already).
//      */
//     void register_subsequent_bug_reporting(PolicyTestingBaseEngine *engine, const State &pool_state, int policy_value);

//     /**
//      * Clears cached_paths; only necessary if caching all policy runs becomes too memory intensive.
//      * Could then e.g. be called after an oracle invocation on a state.
//      * Might be useful to only delete portfolio runs then, to still allow for later classification of pool bugs.
//      *
//      * NOTE: probably not possible given implementation of PerStateInformation; would require a split into PerStateInformation per Policy/"Entity"
//      */
//     // void clear_parent_information();


//     /**
//      * Insert a path where updates should be propagated.
//      *
//      * NOTE: would require some form of path handling apart from policy paths, e.g. with separate PerStateInformation or by not storing "runs" but just parents
//      */
//     // int update_along_path(std::vector<OperatorID> &plan, std::vector<State> &path, int model);
// };
// }
