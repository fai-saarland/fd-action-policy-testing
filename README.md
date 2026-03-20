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

The Policy Comparison Oracle and Multi-Policy Search addition was used in the following publication:
```
@InProceedings{eisenhut-et-al-icaps26, 
    title     = {Automatic Metamorphic Test Oracles for Action-Policy Testing},
    author    = {Sievers, Ben and Eisenhut, Jan and Hoffmann, Jörg},
    booktitle = {Proceedings of the 36th International Conference on Automated Planning and Scheduling ({ICAPS}'26), 2026},
    year      = {2026}
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
This could be of the form `pool_fuzzer(bias=<bias>,oracle=<oracle>)`, 
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
dummy_engine(oracle=metamorphic_oracle(abs=builder_massim(merge_strategy=merge_dfp(),limit_transitions_merge=10000),write_sim_and_exit=true,sim_file="<path/to/sim/file/for/result>",max_simulation_time=1800,max_total_time=7200))
```

#### Oracle Step

In order to run the oracle on a precomputed pool, you could select e.g. this `<search config>`:
```
pool_policy_tester(max_time=<time limit in seconds>,pool_file="<path/to/pool/file>",oracle=<oracle>)
```
where `<oracle>` could be:

```
composite_oracle(qual_oracle=planner_oracle(oracle=internal_planner(conf=ehc_ff,max_planner_time=300)),\
quant_oracle=aras(aras_dir="<path/to/aras/tool>",aras_max_time_limit=300),\
metamorphic_oracle=bound_maintenance_oracle(conduct_lookahead_search=true,lookahead_heuristic=ff(),consider_intermediate_states=true,read_simulation=true,sim_file="<path/to/simulation/file>"))
```

A selection of possible oracles is:

* `aras(...)`
* `composite_oracle(...)`
* `planner_oracle(...)`
* `bound_maintenance_oracle(...)`
* `state_morphing_oracle(...)`
* `policy_comparison_oracle(...)`


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