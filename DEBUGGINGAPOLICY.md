# Fast Downward Fuzzing Extension

@InProceedings{steinmetz-et-al-icaps22,
    author={M. Steinmetz and D. Fišer and H. Eniser and P. Ferber and T. Gros and P. Heim and D. Höller and X. Schuler and V. Wüstholz and M. Christakis and and J. Hoffmann},
    title={Debugging a Policy: Automatic Action-Policy Testing in AI Planning}, 
    booktitle={Proceedings of the 32nd International Conference on Automated Planning and Scheduling ({ICAPS}'22), 2022},
    year={2022}
}

Code is located in: src/search/policy_testing/

# Usage: fuzzing ASNet policies

General parameter structure:

./fast-downward.py domain.pddl problem.pddl \
    --policy "pi=asnet_policy(domain_pddl=domain.pddl, problem_pddl=problem.pddl, snapshot=model.pkl)" \
    --search "pool_fuzzer(eval=hmax(), policy=pi, max_walk_length=5, max_pool_size=200, max_time=36000, bias=plan_length_bias(policy=pi), filter=novelty_filter(2), testing_method=invertible_domain_tester(), seed=1234, pool_file=pool.txt, bugs_file=bugs.txt, random_walks_check_policy_unsolved=true)"

Important parameters: *filter*, *bias*, and *testing_method*

- *filter*: Only states that pass the filter test are added to the pool. Options: (1) If the parameter is omitted, no filter is applied. (2) *filter=novelty_filter(2)* only adds a state to the pool, if the state contains at least one fact pair not contained in any state in the pool, yet.

- *bias*: Controls the random walks of the fuzzer. Options: (1) If parameter is omitted, successor states are sampled uniformly at random. (2) *bias=plan_length_bias(policy=pi)* biases the sampling towards successor states with larger policy plan lengths.

- *testing_method*: Method that checks whether a pool state is a bug. Options: (1) *testing_method=invertible_domain_tester()* assumes domain to be invertible; (2) *testing_method=aras(aras_dir=PATH_TO_ARAS)* uses Aras plan improver; (3) *testing_method=bounded_lookahead_tester(depth=2)* evaluates the policy at all leafs of the *depth*-lookahead-tree; (4) *testing_method=oracle_based_tester(oracle=x)* runs an "oracle" to obtain some plan, and compares that plan to the one given by the policy. *x* can be one of {*external_planner_oracle(downward_path=PATH_TO_FD, params=LIST_OF_PARAMETERS)*, *matching_blocks_qual_oracle()*, *spanner_qual_oracle()*}.


