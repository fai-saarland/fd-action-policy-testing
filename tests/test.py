#!/usr/bin/env python3

import os
import subprocess

test_files_dir = 'sas'
engine = '../builds/debug/bin/downward'

oracle_configs = [
    'atomic_unrelaxation_oracle(abs=builder_atomic(), debug=true)',
    'unrelaxation_oracle(abs=builder_atomic(), debug=true)',
    'iterative_improvement_oracle(conduct_lookahead_search=false, abs=builder_atomic(), debug=true)',
    'iterative_improvement_oracle(lookahead_heuristic=add(), abs=builder_atomic(), debug=true)',
    'iterative_improvement_oracle(lookahead_heuristic=add(), deferred_evaluation=true, abs=builder_atomic(), debug=true)',
    'unrelaxation_oracle(abs=builder_massim(merge_strategy=merge_dfp()), debug=true)',
    'iterative_improvement_oracle(conduct_lookahead_search=false, abs=builder_massim(merge_strategy=merge_dfp()), '
    'debug=true)',
    'iterative_improvement_oracle(lookahead_heuristic=add(), abs=builder_massim(merge_strategy=merge_dfp()),'
    ' debug=true)',
    'iterative_improvement_oracle(conduct_lookahead_search=false, abs=builder_massim(merge_strategy=merge_dfp()), '
    'consider_intermediate_states=true, debug=true)',
    'iterative_improvement_oracle(lookahead_heuristic=add(), abs=builder_massim(merge_strategy=merge_dfp()), '
    'consider_intermediate_states=true, debug=true)',
    'iterative_improvement_oracle(lookahead_heuristic=add(), abs=builder_massim(merge_strategy=merge_dfp()), '
    'consider_intermediate_states=true, deferred_evaluation=true, debug=true)',
    'bounded_lookahead_oracle(max_evaluation_steps=4, dead_end_eval=hmax())',
    'bounded_lookahead_oracle()',
    'estimator_based_oracle(consider_intermediate_states=true, report_parent_bugs=true,'
    'oracle=internal_planner_plan_cost_estimator(conf=ehc_ff))',
    'estimator_based_oracle(report_parent_bugs=true,'
    'oracle=internal_planner_plan_cost_estimator(conf=ehc_ff))',
    'estimator_based_oracle(oracle=internal_planner_plan_cost_estimator(conf=ehc_ff))',
    'estimator_based_oracle(oracle=internal_planner_plan_cost_estimator(conf=astar_lmcut))',
    #'composite_oracle(qual_oracle=estimator_based_oracle(consider_intermediate_states=true,'
    #'oracle=internal_planner_plan_cost_estimator(conf=ehc_ff)), '
    #'quant_oracle=estimator_based_oracle(oracle=internal_planner_plan_cost_estimator(conf=astar_lmcut))'
    #'metamorphic_oracle=iterative_improvement_oracle(conduct_lookahead_search=true, lookahead_heuristic=ff(), '
    #'consider_intermediate_states=true, abs=builder_massim(merge_strategy=merge_dfp()), debug=true))',
    #'composite_oracle(qual_oracle=estimator_based_oracle(consider_intermediate_states=true,'
    #'oracle=internal_planner_plan_cost_estimator(conf=ehc_ff)), '
    #'quant_oracle=estimator_based_oracle(oracle=internal_planner_plan_cost_estimator(conf=astar_lmcut)))'
]

search_configs = [("simplified_pool_fuzzer(policy=pi, eval=hmax(), debug=true, testing_method=" + oracle_config + ")")
                  for oracle_config in oracle_configs]
policies = ["heuristic_descend_policy(eval=lmcut())", "heuristic_descend_policy(eval=lmcut(), steps_limit=4)", "heuristic_descend_policy(eval=lmcut(), steps_limit=10)"]

for search_config in search_configs:
    for pi in policies:
        print(f"Testing search config {search_config} for policy {pi}")
        for instance_name in sorted(os.listdir(test_files_dir)):
            print(f"Testing {instance_name}: ", end="")
            test_file = os.path.join(test_files_dir, instance_name)
            with open(test_file, 'r') as input_file:
                call = subprocess.run(
                    [engine, "--policy", f"pi={pi}", "--search", search_config],
                    stdin=input_file, capture_output=True)
                if call.returncode != 12:
                    print(f"Bad return code {call.returncode}, expected 12")
                    print(f"\nstdout:\n{call.stdout.decode()}\n\nstderr:\n{call.stderr.decode()}")
                    exit(1)
                print("Passed")
