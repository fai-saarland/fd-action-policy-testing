# Fast Downward Action-Policy Testing

This repository contains the source code for action-policy testing as part of the [bughive framework](https://github.com/fai-saarland/bughive/).
The project is still active.
In case you have not cloned this project from or GitHub repository, please check for newer releases [here](https://github.com/fai-saarland/fd-action-policy-testing).

We build on the Fast Downward fuzzing extension by Steinmetz and others (whose source code can be accessed [here](https://doi.org/10.5281/zenodo.6323289)).

```
@InProceedings{steinmetz-et-al-icaps22, 
    title     = {Debugging a Policy: Automatic Action-Policy Testing in AI Planning},
    author    = {Steinmetz, Marcel and Fišer, Daniel and Eniser, Hasan Ferit and Ferber, Patrick and Gros, Timo P. and Heim, Philippe and Höller, Daniel and Schuler, Xandra and Wüstholz, Valentin and Christakis, Maria and Hoffmann, Jörg},
    booktitle = {Proceedings of the 32nd International Conference on Automated Planning and Scheduling ({ICAPS}'22), 2022},
    doi       = {10.1609/icaps.v32i1.19820}, 
    url       = {https://ojs.aaai.org/index.php/ICAPS/article/view/19820}, 
    year      = {2022}
}
```

As of now, there are two publications in connection with this project:
```
@InProceedings{eisenhut-et-al-icaps23, 
    title     = {Automatic Metamorphic Test Oracles for Action-Policy Testing},
    author    = {Eisenhut, Jan and Torralba, Álvaro and Christakis, Maria and Hoffmann, Jörg},
    booktitle = {Proceedings of the 33rd International Conference on Automated Planning and Scheduling ({ICAPS}'23), 2023},
    year      = {2023}
}

@InProceedings{eisenhut-et-al-icaps24, 
    title     = {New Fuzzing Biases for Action Policy Testing},
    author    = {Eisenhut, Jan and Schuler, Xandra and Fišer, Daniel and Höller, Daniel and Christakis, Maria and Hoffmann, Jörg},
    booktitle = {Proceedings of the 44th International Conference on Automated Planning and Scheduling ({ICAPS}'24), 2024},
    year      = {2024}
}
```

The computation of dominance functions used for metamorphic oracles is based on:
```
@InProceedings{torralba-ijcai2017,
  author    = {Torralba, Álvaro},
  title     = {From Qualitative to Quantitative Dominance Pruning for Optimal Planning},
  booktitle = {Proceedings of the Twenty-Sixth International Joint Conference on Artificial Intelligence, {IJCAI-17}},
  pages     = {4426--4432},
  doi       = {10.24963/ijcai.2017/618},
  url       = {https://doi.org/10.24963/ijcai.2017/618},
  year      = {2017},
}
```

## Building the Project

Please build the project as a part of the [bughive framework](https://github.com/fai-saarland/bughive/) using the Makefile provided there. 
The Makefile target is `fd-action-policy-testing`.
We only support Linux.

## Code

The related to policy testing is primarily located in `src/search/policy_testing/`. 
The (adapted) source code for the computation of dominance functions can be found in `src/search/policy_testing/simulations/`.

## Usage

Check out the test drivers in [the bughive repository](https://github.com/fai-saarland/bughive/tree/master/test_drivers) to learn how to easily invoke the tool.

As search configuration you must specify a search algorithm such as `pool_fuzzer`.
This could be of the form `pool_fuzzer(bias=<bias>,testing_method=<oracle>)`, 
where `<bias>` is one determines the fuzzing bias to be used in the fuzzing step (if one shall be used)
and `<oracle>` is the oracle to be used in the oracle step.

Our tool supports executing all steps of the testing pipeline in one go, however they can also be split and intermediate results can be saved, e.g., by writing pool files to disk.

#### Fuzzing Step

Here is an example for `<search config>` for writing a pool file.

```
pool_fuzzer(max_steps=10000000,max_pool_size=100,eval=hmax(),max_walk_length=5,pool_file="<path/to/pool/file/for/result>",penalize_policy_fails=true,bias_budget=200,cache_bias=false[,bias=<bias>])
```
where bias=<bias> must be omitted if no bias is to be used. 

Here are a few example bias configs:

* policy quality bias: `plan_length_bias(horizon=50)`
* detour bias with FF: `detour_bias(h=ff(),horizon=50,omit_maximization=false)`
* detour bias with hstar: `detour_bias(horizon=50,ipo=internal_planner_plan_cost_estimator(conf=astar_lmcut,continue_after_time_out=false),omit_maximization=true)`
* surface bias with FF: `surface_bias(h=ff(),horizon=50, omit_maximization=false)`
* surface bias with hstar: `surface_bias(horizon=50,ipo=internal_planner_plan_cost_estimator(conf=astar_lmcut,continue_after_time_out=false),omit_maximization=true)`
* loopiness bias with FF: `loopiness_bias(h=ff(), horizon=50,omit_maximization=false)`
* loopiness bias with hstar: `loopiness_bias(horizon=50,ipo=internal_planner_plan_cost_estimator(conf=astar_lmcut,continue_after_time_out=false),omit_maximization=false)`

#### Precomputing Dominance Functions

If you use an oracle that requires computing a dominance function, you might want to precompute it and pool_store it in a simulation file (in order to load it multiple times).
You can achieve that by selecting `<search config>` to:

```
dummy_engine(testing_method=numeric_dominance_oracle(abs=builder_massim(merge_strategy=merge_dfp(),limit_transitions_merge=10000),write_sim_and_exit=true,sim_file="<path/to/sim/file/for/result>",max_simulation_time=1800,max_total_time=7200))
```

#### Oracle Step

In order to run the oracle on a precomputed pool, you could select e.g. this `<search config>`:
```
pool_policy_tester(max_time=<time limit in seconds>,pool_file="<path/to/pool/file>",testing_method=<oracle>)
```
where `<oracle>` could be:

```
composite_oracle(qual_oracle=estimator_based_oracle(oracle=internal_planner_plan_cost_estimator(conf=ehc_ff,max_planner_time=300)),\
quant_oracle=aras(aras_dir="<path/to/aras/tool>",aras_max_time_limit=300),\
metamorphic_oracle=iterative_improvement_oracle(conduct_lookahead_search=true,lookahead_heuristic=ff(),consider_intermediate_states=true,read_simulation=true,sim_file="<path/to/simulation/file>"))
```

A selection of possible oracles is:

* `aras(...)`
* `composite_oracle(...)`
* `estimator_based_oracle(...)`
* `iterative_improvement_oracle(...)`
* `unrelaxation_oracle(...)`

`iterative_improvement_oracle` implements the bound maintenance oracles (BMOs), while `atomic_unrelaxation_oracle` and `unrelaxation_oracle` implement state morphing oracles (SMOs).

#### Aras Oracle

In order to use the Aras oracle, you must first build the Aras plan improvement tool described here:

```
@inproceedings{nakhost:mueller:icaps-10,
  author    = {Hootan Nakhost and Martin Müller},
  title     = {Action Elimination and Plan Neighborhood Graph Search: Two Algorithms for Plan Improvement},
  booktitle = {Proceedings of the 20th International Conference on Automated Planning and Scheduling ({ICAPS'10})},
  pages     = {121--128},
  year      = {2010}
}
```

We included the source code of a version of it in `resources` (slightly modified to fix compiler errors).
You can build it by running `cd resources; unzip aras.zip -d aras && cd aras/src && ./build_all`.

### Detailed Options

You can learn about the supported configuration options by calling `./builds/release/bin/downward --help`.
Below is an excerpt of the most relevant parts.
To learn about the options of a specific feature (e.g., `pool_fuzzer`) only you can use `./builds/release/bin/downward --help pool_fuzzer`.

```
Help for FuzzingBias

== detour_bias ==
detour_bias(h=<none>, ipo=<none>, omit_maximization=false, policy=<none>, horizon=50)
 h (Evaluator): heuristic (required if no ipo is given)
 ipo (plan_cost_estimator): plan cost estimator (e.g. to compute h*)
 omit_maximization (bool): do not maximize over all sub-paths, only consider first and last state
 policy (PolicyForTesting): policy to test (omit if global remote policy is set)
 horizon (int): number of policy steps to consider in bias computation; choose 0 or negative value to set no limit
== heuristic_bias ==
heuristic_bias(h)
 h (Evaluator): heuristic; only implemented for safe heuristics (if the heuristic returns infinity a bias of negative infinity will be chosen).
== internal_planner_oracle_bias ==
internal_planner_oracle_bias(internal_planner_oracle)
 internal_planner_oracle (plan_cost_estimator): plan cost estimator (e.g. to compute h*)
== loopiness_bias ==
loopiness_bias(h=<none>, ipo=<none>, omit_maximization=false, omit_maximization_if_task_invertible=false, policy=<none>, horizon=50)
 h (Evaluator): heuristic (required if no ipo is given)
 ipo (plan_cost_estimator): plan cost estimator (e.g. to compute h*)
 omit_maximization (bool): do not maximize over all sub-paths, only consider first and last state
 omit_maximization_if_task_invertible (bool): omit maximization if task is invertible
 policy (PolicyForTesting): policy to test (omit if global remote policy is set)
 horizon (int): number of policy steps to consider in bias computation; choose 0 or negative value to set no limit
== plan_length_bias ==
plan_length_bias(policy=<none>, horizon=50)
 policy (PolicyForTesting): policy to test (omit if global remote policy is set)
 horizon (int): number of policy steps to consider in bias computation; choose 0 or negative value to set no limit
== surface_bias ==
surface_bias(h=<none>, ipo=<none>, omit_maximization=false, policy=<none>, horizon=50)
 h (Evaluator): heuristic (required if no ipo is given)
 ipo (plan_cost_estimator): plan cost estimator (e.g. to compute h*)
 omit_maximization (bool): do not maximize over all sub-paths, only consider first and last state
 policy (PolicyForTesting): policy to test (omit if global remote policy is set)
 horizon (int): number of policy steps to consider in bias computation; choose 0 or negative value to set no limit

Help for Oracle

== aras ==
aras(debug=false, report_parent_bugs=false, consider_intermediate_states=false, enforce_intermediate=false, aras_dir, aras_max_time_limit=14400, cache_results=true)
 debug (bool): Run in (very costly) debug mode.
 report_parent_bugs (bool): For every reported bug go through all policy parents and report them as bugs as well.
 consider_intermediate_states (bool): Run bug test also on intermediate states.
 enforce_intermediate (bool): Consider intermediate states even if bug candidate is known to be a bug
 aras_dir (std::__cxx11::basic_string<char>): Base directory of the ARAS plan improver
 aras_max_time_limit (int): Maximal time to run ARAS.
 cache_results (bool): Cache the results of oracle invocations
== atomic_unrelaxation_oracle ==
atomic_unrelaxation_oracle(debug=false, report_parent_bugs=false, consider_intermediate_states=false, enforce_intermediate=false, abs=builder_massim(merge_strategy=merge_dfp(), limit_transitions_merge=10000), tau_labels_recursive=true, tau_labels_self_loops=true, tau_labels_noop=false, truncate_value=1000, max_simulation_time=1800, min_simulation_time=1, max_total_time=7200, max_lts_size_to_compute_simulation=1000000, num_labels_to_use_dominates_in=0, dump=false, local_bug_test=ALL, sim_file=<none>, write_sim_and_exit=false, read_simulation=false, test_serialization=false, operations_per_state=4, max_evaluation_steps=-1, dead_end_eval=<none>)
 debug (bool): Run in (very costly) debug mode.
 report_parent_bugs (bool): For every reported bug go through all policy parents and report them as bugs as well.
 consider_intermediate_states (bool): Run bug test also on intermediate states.
 enforce_intermediate (bool): Consider intermediate states even if bug candidate is known to be a bug
 abs (AbstractionBuilder): abstraction builder
 tau_labels_recursive (bool): Use stronger notion of tau labels based on self loops everywhere
 tau_labels_self_loops (bool): Use stronger notion of tau labels based on self loops everywhere
 tau_labels_noop (bool): Use stronger notion of tau labels based on noop
 truncate_value (int): Assume -infinity if below minus this value
 max_simulation_time (int): Maximum number of seconds spent in computing a single update of a simulation
 min_simulation_time (int): Minimum number of seconds spent in computing a single update of a simulation
 max_total_time (int): Maximum number of seconds spent in computing all updates of a simulation
 max_lts_size_to_compute_simulation (int): Avoid computing simulation on ltss that have more states than this number
 num_labels_to_use_dominates_in (int): Use dominates_in for instances that have less than this amount of labels
 dump (bool): Dumps the relation that has been found
 local_bug_test ({none, one, all}): Apply local bug test not at all (NONE), only for the state it is called for (ONE) or for all states in the path induced by executing the policy on the state (ALL)
 - NONE: Do not apply local bug tests at all.
 - ONE: Only apply local bug test for test state itself.
 - ALL: Apply local bug test for all states in the considered partial policy trace.
 sim_file (std::__cxx11::basic_string<char>): The file to write a computed simulation to or to read a simulation from.
 write_sim_and_exit (bool): Only compute the specified dominance function, write it to the sim_file and exit.
 read_simulation (bool): Read simulation from sim_file instead of computing it.
 test_serialization (bool): Write simulation to disk, read it and make sure it coincides.
 operations_per_state (int): Number of unrelaxations to check in each state. Values smaller than 1 will be set to 1.
 max_evaluation_steps (int): Maximal number of steps in evaluation of policy in unrelaxed state.
 dead_end_eval (Evaluator): Evaluator used for dead end detection in policy evaluation of dead end states.
== bounded_lookahead_oracle ==
bounded_lookahead_oracle(debug=false, report_parent_bugs=false, consider_intermediate_states=false, enforce_intermediate=false, depth=2, max_evaluation_steps=-1, dead_end_eval=<none>, cache_results=true)
 debug (bool): Run in (very costly) debug mode.
 report_parent_bugs (bool): For every reported bug go through all policy parents and report them as bugs as well.
 consider_intermediate_states (bool): Run bug test also on intermediate states.
 enforce_intermediate (bool): Consider intermediate states even if bug candidate is known to be a bug
 depth (int): Depth limit.
 max_evaluation_steps (int): Maximal number of steps in evaluation of policy in unrelaxed state.
 dead_end_eval (Evaluator): Evaluator used for dead end detection in policy evaluation of dead end states.
 cache_results (bool): Cache the results of oracle invocations
== composite_oracle ==
composite_oracle(debug=false, report_parent_bugs=false, consider_intermediate_states=false, enforce_intermediate=false, qual_oracle=<none>, quant_oracle=<none>, metamorphic_oracle=<none>, enforce_external=false)
 debug (bool): Run in (very costly) debug mode.
 report_parent_bugs (bool): For every reported bug go through all policy parents and report them as bugs as well.
 consider_intermediate_states (bool): Run bug test also on intermediate states.
 enforce_intermediate (bool): Consider intermediate states even if bug candidate is known to be a bug
 qual_oracle (Oracle): oracle for qualitative evaluation
 quant_oracle (Oracle): oracle for quantitative evaluation
 metamorphic_oracle (Oracle): oracle for metamorphic testing
 enforce_external (bool): run external oracle(s) on intermediate states even if pool state could be confirmed as a bug by metamorphic oracle
== estimator_based_oracle ==
estimator_based_oracle(debug=false, report_parent_bugs=false, consider_intermediate_states=false, enforce_intermediate=false, oracle, cache_results=true)
 debug (bool): Run in (very costly) debug mode.
 report_parent_bugs (bool): For every reported bug go through all policy parents and report them as bugs as well.
 consider_intermediate_states (bool): Run bug test also on intermediate states.
 enforce_intermediate (bool): Consider intermediate states even if bug candidate is known to be a bug
 oracle (plan_cost_estimator): Plan cost estimator.
 cache_results (bool): Cache the results of oracle invocations
== invertible_domain_oracle ==
invertible_domain_oracle(debug=false, report_parent_bugs=false, consider_intermediate_states=false, enforce_intermediate=false)
 debug (bool): Run in (very costly) debug mode.
 report_parent_bugs (bool): For every reported bug go through all policy parents and report them as bugs as well.
 consider_intermediate_states (bool): Run bug test also on intermediate states.
 enforce_intermediate (bool): Consider intermediate states even if bug candidate is known to be a bug
== iterative_improvement_oracle ==
iterative_improvement_oracle(debug=false, report_parent_bugs=false, consider_intermediate_states=false, enforce_intermediate=false, abs=builder_massim(merge_strategy=merge_dfp(), limit_transitions_merge=10000), tau_labels_recursive=true, tau_labels_self_loops=true, tau_labels_noop=false, truncate_value=1000, max_simulation_time=1800, min_simulation_time=1, max_total_time=7200, max_lts_size_to_compute_simulation=1000000, num_labels_to_use_dominates_in=0, dump=false, local_bug_test=ALL, sim_file=<none>, write_sim_and_exit=false, read_simulation=false, test_serialization=false, max_state_comparisons=1000000, max_lookahead_state_comparisons=1000000, conduct_lookahead_search=true, update_parents=true, lookahead_heuristic=<none>, deferred_evaluation=false, domain_unit_cost_and_invertible=false, max_lookahead_state_visits=100, lookahead_comp=h)
 debug (bool): Run in (very costly) debug mode.
 report_parent_bugs (bool): For every reported bug go through all policy parents and report them as bugs as well.
 consider_intermediate_states (bool): Run bug test also on intermediate states.
 enforce_intermediate (bool): Consider intermediate states even if bug candidate is known to be a bug
 abs (AbstractionBuilder): abstraction builder
 tau_labels_recursive (bool): Use stronger notion of tau labels based on self loops everywhere
 tau_labels_self_loops (bool): Use stronger notion of tau labels based on self loops everywhere
 tau_labels_noop (bool): Use stronger notion of tau labels based on noop
 truncate_value (int): Assume -infinity if below minus this value
 max_simulation_time (int): Maximum number of seconds spent in computing a single update of a simulation
 min_simulation_time (int): Minimum number of seconds spent in computing a single update of a simulation
 max_total_time (int): Maximum number of seconds spent in computing all updates of a simulation
 max_lts_size_to_compute_simulation (int): Avoid computing simulation on ltss that have more states than this number
 num_labels_to_use_dominates_in (int): Use dominates_in for instances that have less than this amount of labels
 dump (bool): Dumps the relation that has been found
 local_bug_test ({none, one, all}): Apply local bug test not at all (NONE), only for the state it is called for (ONE) or for all states in the path induced by executing the policy on the state (ALL)
 - NONE: Do not apply local bug tests at all.
 - ONE: Only apply local bug test for test state itself.
 - ALL: Apply local bug test for all states in the considered partial policy trace.
 sim_file (std::__cxx11::basic_string<char>): The file to write a computed simulation to or to read a simulation from.
 write_sim_and_exit (bool): Only compute the specified dominance function, write it to the sim_file and exit.
 read_simulation (bool): Read simulation from sim_file instead of computing it.
 test_serialization (bool): Write simulation to disk, read it and make sure it coincides.
 max_state_comparisons (int): Maximal number of states to compare bug candidates to
 max_lookahead_state_comparisons (int): Maximal number of states to compare bug candidates to withing lookahead search
 conduct_lookahead_search (bool): Enables lookahead search
 update_parents (bool): Pass cost bounds to policy parent states
 lookahead_heuristic (Evaluator): Heuristic to be used in lookahead search.
 deferred_evaluation (bool): Defer heuristic evaluation in lookahead_search. Not implemented in qual_lookahead_search yet.
 domain_unit_cost_and_invertible (bool): Performs optimizations assuming that the task is unit cost and the domain is invertible.
 max_lookahead_state_visits (int): Maximal number of states visited in lookahead search
 lookahead_comp ({h, g_plus_h}): The comparator to be used in lookahead search; h (resembles GBFS) or g+h (resembles A*)
 - h: heuristic value only (resembles GBFS).
 - g_plus_h: f=g+h (resembles A*)
== numeric_dominance_oracle ==
numeric_dominance_oracle(debug=false, report_parent_bugs=false, consider_intermediate_states=false, enforce_intermediate=false, abs=builder_massim(merge_strategy=merge_dfp(), limit_transitions_merge=10000), tau_labels_recursive=true, tau_labels_self_loops=true, tau_labels_noop=false, truncate_value=1000, max_simulation_time=1800, min_simulation_time=1, max_total_time=7200, max_lts_size_to_compute_simulation=1000000, num_labels_to_use_dominates_in=0, dump=false, local_bug_test=ALL, sim_file=<none>, write_sim_and_exit=false, read_simulation=false, test_serialization=false)
 debug (bool): Run in (very costly) debug mode.
 report_parent_bugs (bool): For every reported bug go through all policy parents and report them as bugs as well.
 consider_intermediate_states (bool): Run bug test also on intermediate states.
 enforce_intermediate (bool): Consider intermediate states even if bug candidate is known to be a bug
 abs (AbstractionBuilder): abstraction builder
 tau_labels_recursive (bool): Use stronger notion of tau labels based on self loops everywhere
 tau_labels_self_loops (bool): Use stronger notion of tau labels based on self loops everywhere
 tau_labels_noop (bool): Use stronger notion of tau labels based on noop
 truncate_value (int): Assume -infinity if below minus this value
 max_simulation_time (int): Maximum number of seconds spent in computing a single update of a simulation
 min_simulation_time (int): Minimum number of seconds spent in computing a single update of a simulation
 max_total_time (int): Maximum number of seconds spent in computing all updates of a simulation
 max_lts_size_to_compute_simulation (int): Avoid computing simulation on ltss that have more states than this number
 num_labels_to_use_dominates_in (int): Use dominates_in for instances that have less than this amount of labels
 dump (bool): Dumps the relation that has been found
 local_bug_test ({none, one, all}): Apply local bug test not at all (NONE), only for the state it is called for (ONE) or for all states in the path induced by executing the policy on the state (ALL)
 - NONE: Do not apply local bug tests at all.
 - ONE: Only apply local bug test for test state itself.
 - ALL: Apply local bug test for all states in the considered partial policy trace.
 sim_file (std::__cxx11::basic_string<char>): The file to write a computed simulation to or to read a simulation from.
 write_sim_and_exit (bool): Only compute the specified dominance function, write it to the sim_file and exit.
 read_simulation (bool): Read simulation from sim_file instead of computing it.
 test_serialization (bool): Write simulation to disk, read it and make sure it coincides.
== sequence_oracle ==
sequence_oracle(debug=false, report_parent_bugs=false, consider_intermediate_states=false, enforce_intermediate=false, first_oracle, second_oracle)
 debug (bool): Run in (very costly) debug mode.
 report_parent_bugs (bool): For every reported bug go through all policy parents and report them as bugs as well.
 consider_intermediate_states (bool): Run bug test also on intermediate states.
 enforce_intermediate (bool): Consider intermediate states even if bug candidate is known to be a bug
 first_oracle (Oracle): oracle to be invoked first
 second_oracle (Oracle): oracle to be invoked second
== unrelaxation_oracle ==
unrelaxation_oracle(debug=false, report_parent_bugs=false, consider_intermediate_states=false, enforce_intermediate=false, abs=builder_massim(merge_strategy=merge_dfp(), limit_transitions_merge=10000), tau_labels_recursive=true, tau_labels_self_loops=true, tau_labels_noop=false, truncate_value=1000, max_simulation_time=1800, min_simulation_time=1, max_total_time=7200, max_lts_size_to_compute_simulation=1000000, num_labels_to_use_dominates_in=0, dump=false, local_bug_test=ALL, sim_file=<none>, write_sim_and_exit=false, read_simulation=false, test_serialization=false, operations_per_state=4, max_evaluation_steps=-1, dead_end_eval=<none>)
 debug (bool): Run in (very costly) debug mode.
 report_parent_bugs (bool): For every reported bug go through all policy parents and report them as bugs as well.
 consider_intermediate_states (bool): Run bug test also on intermediate states.
 enforce_intermediate (bool): Consider intermediate states even if bug candidate is known to be a bug
 abs (AbstractionBuilder): abstraction builder
 tau_labels_recursive (bool): Use stronger notion of tau labels based on self loops everywhere
 tau_labels_self_loops (bool): Use stronger notion of tau labels based on self loops everywhere
 tau_labels_noop (bool): Use stronger notion of tau labels based on noop
 truncate_value (int): Assume -infinity if below minus this value
 max_simulation_time (int): Maximum number of seconds spent in computing a single update of a simulation
 min_simulation_time (int): Minimum number of seconds spent in computing a single update of a simulation
 max_total_time (int): Maximum number of seconds spent in computing all updates of a simulation
 max_lts_size_to_compute_simulation (int): Avoid computing simulation on ltss that have more states than this number
 num_labels_to_use_dominates_in (int): Use dominates_in for instances that have less than this amount of labels
 dump (bool): Dumps the relation that has been found
 local_bug_test ({none, one, all}): Apply local bug test not at all (NONE), only for the state it is called for (ONE) or for all states in the path induced by executing the policy on the state (ALL)
 - NONE: Do not apply local bug tests at all.
 - ONE: Only apply local bug test for test state itself.
 - ALL: Apply local bug test for all states in the considered partial policy trace.
 sim_file (std::__cxx11::basic_string<char>): The file to write a computed simulation to or to read a simulation from.
 write_sim_and_exit (bool): Only compute the specified dominance function, write it to the sim_file and exit.
 read_simulation (bool): Read simulation from sim_file instead of computing it.
 test_serialization (bool): Write simulation to disk, read it and make sure it coincides.
 operations_per_state (int): Number of unrelaxations to check in each state. Values smaller than 1 will be set to 1.
 max_evaluation_steps (int): Maximal number of steps in evaluation of policy in unrelaxed state.
 dead_end_eval (Evaluator): Evaluator used for dead end detection in policy evaluation of dead end states.

Help for PolicyForTesting

This feature type can be bound to variables using ``let(variable_name, variable_definition, expression)`` where ``expression`` can use ``variable_name``. Predefinitions using ``--evaluator``, ``--heuristic``, and ``--landmarks`` are automatically transformed into ``let``-expressions but are deprecated.
== cached_policy ==
cached_policy(steps_limit=0)
 steps_limit (int): The maximal number of steps to execute the policy. 0 or negative value means no limit
== heuristic_descend_policy ==
heuristic_descend_policy(steps_limit=0, eval, strictly_descend=false, stop_at_dead_ends=true)
 steps_limit (int): The maximal number of steps to execute the policy. 0 or negative value means no limit
 eval (Evaluator): The heuristic
 strictly_descend (bool): Descend strictly
 stop_at_dead_ends (bool): Stop at dead ends
== hill_climbing_policy ==
hill_climbing_policy(steps_limit=0, eval, helpful_actions_pruning=false)
 steps_limit (int): The maximal number of steps to execute the policy. 0 or negative value means no limit
 eval (Evaluator): The heuristic
 helpful_actions_pruning (bool): Apply helpful actions pruning
== remote_policy ==
remote_policy(steps_limit=0)
 steps_limit (int): The maximal number of steps to execute the policy. 0 or negative value means no limit

Help for SearchAlgorithm

== A* search (eager) ==
astar(eval, lazy_evaluator=<none>, pruning=null(), cost_type=normal, bound=infinity, max_time=infinity, transform=<none>, verbosity=normal)
 eval (Evaluator): evaluator for h-value
 lazy_evaluator (Evaluator): An evaluator that re-evaluates a state before it is expanded.
 pruning (PruningMethod): Pruning methods can prune or reorder the set of applicable operators in each state and thereby influence the number and order of successor states that are considered.
 cost_type ({normal, one, plusone}): Operator cost adjustment type. No matter what this setting is, axioms will always be considered as actions of cost 0 by the heuristics that treat axioms as actions.
 - normal: all actions are accounted for with their real cost
 - one: all actions are accounted for as unit cost
 - plusone: all actions are accounted for as their real cost + 1 (except if all actions have original cost 1, in which case cost 1 is used). This is the behaviour known for the heuristics of the LAMA planner. This is intended to be used by the heuristics, not search algorithms, but is supported for both.
 bound (int): exclusive depth bound on g-values. Cutoffs are always performed according to the real cost, regardless of the cost_type parameter
 max_time (double): maximum time in seconds the search is allowed to run for. The timeout is only checked after each complete search step (usually a node expansion), so the actual runtime can be arbitrarily longer. Therefore, this parameter should not be used for time-limiting experiments. Timed-out searches are treated as failed searches, just like incomplete search algorithms that exhaust their search space.
 transform (AbstractTask): Optional task transformation for the search algorithm.
 verbosity ({silent, normal, verbose, debug}): Option to specify the verbosity level.
 - silent: only the most basic output
 - normal: relevant information to monitor progress
 - verbose: full output
 - debug: like verbose with additional debug output
== pool_fuzzer ==
pool_fuzzer(max_walk_length=5, pool_file=<none>, bias=<none>, filter=<none>, eval=<none>, novelty_statistics=2, max_pool_size=infinity, max_steps=infinity, penalize_policy_fails=false, seed=1734, bias_budget=200, cache_bias=false, policy=<none>, run_without_policy=false, testing_method=<none>, policy_cache_file=<none>, bugs_file=<none>, read_policy_cache=false, just_write_policy_cache=false, debug=false, verbose=false, abstain_if_first_state_not_known_solved=false, print_bug_states=false, cost_type=normal, bound=infinity, max_time=infinity, transform=<none>, verbosity=normal)
 max_walk_length (int): Maximal length of policy walks.
 pool_file (std::__cxx11::basic_string<char>): Path to pool file (optional).
 bias (FuzzingBias): Fuzzing bias (optional)
 filter (PoolFilter): Pool filter (optional).
 eval (Evaluator): Dead end heuristic (optional).
 novelty_statistics (int): Maximal arity for novelty_store statistics.
 max_pool_size (int): Maximal pool size.
 max_steps (int): Maximal number of fuzzing steps.
 penalize_policy_fails (bool): Uses a bias of infinity if the policy is known to fail on the state;only applied if policy is executed in bias computation
 seed (int): Random seed.
 bias_budget (int): Budget for bias computation in each state expansion; choose 0 to set no limit
 cache_bias (bool): indicates whether the bias should be cached for each state
 policy (PolicyForTesting): The policy to test (optional). Also consider using global a global remote policy.
 run_without_policy (bool): Run engine without policy.
 testing_method (Oracle): The oracle to be used.
 policy_cache_file (std::__cxx11::basic_string<char>): Policy cache file to write to or read from.
 bugs_file (std::__cxx11::basic_string<char>): Path to bugs file.
 read_policy_cache (bool): Read policy cache instead of running the policy (requires policy_cache_file)
 just_write_policy_cache (bool): Skip any calls to oracles (and thus the actual testing), just write the policy cache into the provided cache file.
 debug (bool): Run in (very expensive) debug mode.
 verbose (bool): More verbose output for debugging.
 abstain_if_first_state_not_known_solved (bool): Abort the testing if the first tested state is not solved (possibly within the provided step limit)
 print_bug_states (bool): Print out all found bug states including state values
 cost_type ({normal, one, plusone}): Operator cost adjustment type. No matter what this setting is, axioms will always be considered as actions of cost 0 by the heuristics that treat axioms as actions.
 - normal: all actions are accounted for with their real cost
 - one: all actions are accounted for as unit cost
 - plusone: all actions are accounted for as their real cost + 1 (except if all actions have original cost 1, in which case cost 1 is used). This is the behaviour known for the heuristics of the LAMA planner. This is intended to be used by the heuristics, not search algorithms, but is supported for both.
 bound (int): exclusive depth bound on g-values. Cutoffs are always performed according to the real cost, regardless of the cost_type parameter
 max_time (double): maximum time in seconds the search is allowed to run for. The timeout is only checked after each complete search step (usually a node expansion), so the actual runtime can be arbitrarily longer. Therefore, this parameter should not be used for time-limiting experiments. Timed-out searches are treated as failed searches, just like incomplete search algorithms that exhaust their search space.
 transform (AbstractTask): Optional task transformation for the search algorithm.
 verbosity ({silent, normal, verbose, debug}): Option to specify the verbosity level.
 - silent: only the most basic output
 - normal: relevant information to monitor progress
 - verbose: full output
 - debug: like verbose with additional debug output
== pool_policy_tester ==
pool_policy_tester(policy=<none>, run_without_policy=false, testing_method, policy_cache_file=<none>, bugs_file=<none>, read_policy_cache=false, just_write_policy_cache=false, debug=false, verbose=false, abstain_if_first_state_not_known_solved=false, print_bug_states=false, cost_type=normal, bound=infinity, max_time=infinity, transform=<none>, verbosity=normal, pool_file, start_from=0, max_steps=infinity, novelty_statistics=2)
 policy (PolicyForTesting): The policy to test (optional). Also consider using global a global remote policy.
 run_without_policy (bool): Run engine without policy.
 testing_method (Oracle): The oracle to be used.
 policy_cache_file (std::__cxx11::basic_string<char>): Policy cache file to write to or read from.
 bugs_file (std::__cxx11::basic_string<char>): Path to bugs file.
 read_policy_cache (bool): Read policy cache instead of running the policy (requires policy_cache_file)
 just_write_policy_cache (bool): Skip any calls to oracles (and thus the actual testing), just write the policy cache into the provided cache file.
 debug (bool): Run in (very expensive) debug mode.
 verbose (bool): More verbose output for debugging.
 abstain_if_first_state_not_known_solved (bool): Abort the testing if the first tested state is not solved (possibly within the provided step limit)
 print_bug_states (bool): Print out all found bug states including state values
 cost_type ({normal, one, plusone}): Operator cost adjustment type. No matter what this setting is, axioms will always be considered as actions of cost 0 by the heuristics that treat axioms as actions.
 - normal: all actions are accounted for with their real cost
 - one: all actions are accounted for as unit cost
 - plusone: all actions are accounted for as their real cost + 1 (except if all actions have original cost 1, in which case cost 1 is used). This is the behaviour known for the heuristics of the LAMA planner. This is intended to be used by the heuristics, not search algorithms, but is supported for both.
 bound (int): exclusive depth bound on g-values. Cutoffs are always performed according to the real cost, regardless of the cost_type parameter
 max_time (double): maximum time in seconds the search is allowed to run for. The timeout is only checked after each complete search step (usually a node expansion), so the actual runtime can be arbitrarily longer. Therefore, this parameter should not be used for time-limiting experiments. Timed-out searches are treated as failed searches, just like incomplete search algorithms that exhaust their search space.
 transform (AbstractTask): Optional task transformation for the search algorithm.
 verbosity ({silent, normal, verbose, debug}): Option to specify the verbosity level.
 - silent: only the most basic output
 - normal: relevant information to monitor progress
 - verbose: full output
 - debug: like verbose with additional debug output
 pool_file (std::__cxx11::basic_string<char>): The pool file to load.
 start_from (int): Index of first step to test.
 max_steps (int): Number of pool states to test.
 novelty_statistics (int): Maximal arity for novelty_store statistics.

Help for plan_cost_estimator

== internal_planner_plan_cost_estimator ==
internal_planner_plan_cost_estimator(conf, print_output=false, print_plan=false, max_planner_time=14400, continue_after_time_out=true)
 conf ({astar_lmcut, ehc_ff}): search algorithm, possible choices: astar_lmcut, ehc_ff
 - astar_lmcut: A* with LM-Cut heuristic.
 - ehc_ff: Enforced hill climbing with FF heuristic.
 print_output (bool): Print search output.
 print_plan (bool): Print plan.
 max_planner_time (int): Maximal time to run internal planner.
 continue_after_time_out (bool): Continue testing if internal planner oracle ran into a timeout (or runs out of memory).
```
