# Automatic Metamorphic Test Oracles for Action-Policy Testing

This repository contains the source code and the benchmarks of the paper "Automatic Metamorphic Test Oracles for Action-Policy Testing" accepted at the ICAPS 2023 conference.

```
@InProceedings{eisenhut-et-al-icaps23, 
    title     = {Automatic Metamorphic Test Oracles for Action-Policy Testing},
    author    = {Eisenhut, Jan and Torralba, Álvaro and Christakis, Maria and Hoffmann, Jörg},
    booktitle = {Proceedings of the 33rd International Conference on Automated Planning and Scheduling ({ICAPS}'23), 2023},
    year      = {2023}
}
```

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

The computation of dominance functions is based on:

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

Currently, we only support Linux. We tested building the project with `clang++ 15.0` as well as `g++ 12.2`.
In case some of the following packages are not already installed on your system, please install them, e.g. by running 
```shell 
sudo apt install cmake python3 python3-pip libssl-dev autoconf automake libtool flex bison
```
Additionally, you need to have [boost](https://www.boost.org/) installed. The minimum required version is `1.78`.

In order to download and install everything required to run ASNets, you may run `setup-asnets.sh`.
This should set up everything in the `resources` directory including a python virtual environment `asnet-env`. This may take some time.
For building and using our tool, you need to make sure that `asnet-env` is activated.
To activate it, you may run `source resources/asnets/asnets/asnet-env/bin/activate` (in this directory).

For building the actual tool, please run the `build.py` script. 
Some configurations are predefined, e.g., you may run `./build.py debug` to get a debug build or `./build.py release` to get a release build.

## Code and Benchmarks

The related to policy testing is primarily located in `src/search/policy_testing/`. 
The source code for the computation of dominance functions can be found in `src/search/policy_testing/simulations/`.
We made slight modifications to the code base since we ran the experiments for the paper, e.g., to make sure one can compile the source code using more recent compilers.

## Usage

The general usage of the tool is as follows:

```shell
./fast-downward.py domain.pddl problem.pddl \
--policy "pi=asnet_policy(domain_pddl=domain.pddl, problem_pddl=problem.pddl, snapshot=model.pkl)" \
--search "pool_fuzzer(policy=pi, testing_method=<oracle>)"
```
where `<oracle>` is one of the following implemented oracles. 
`iterative_improvement_oracle` implements the bound maintenance oracles (BMOs), while `atomic_unrelaxation_oracle` and `unrelaxation_oracle` implement state morphing oracles (SMOs).
* `aras(...)`
* `atomic_unrelaxation_oracle(...)`
* `bounded_lookahead_oracle(...)`
* `composite_oracle(...)`
* `dummy_oracle(...)`
* `estimator_based_oracle(...)`
* `iterative_improvement_oracle(...)`
* `unrelaxation_oracle(...)`

You can learn about the options supported by an oracle (say `unrelaxation_oracle`) by calling `./fast-downward.py --search -- --help unrelaxation_oracle`.
You can also run `./fast-downward.py --search -- --help unrelaxation_oracle` to get an overview of all implemented options of the entire tool.

Please note that our tool currently only supports **unit-cost** domains.

#### Usage Example
```shell
./fast-downward.py domain.pddl problem.pddl \
--policy "pi=asnet_policy(domain_pddl=domain.pddl, problem_pddl=problem.pddl, snapshot=model.pkl)" \
--search "pool_fuzzer(policy=pi, max_steps=10000000, max_pool_size=200, testing_method=iterative_improvement_oracle(abs=builder_massim(merge_strategy=merge_dfp(), limit_transitions_merge=10000)))"
```

#### Splitting the Testing Process into Multiple Steps
The tool supports splitting the testing process into multiple steps, i.e., computing the dominance function, building up the pool and (to a certain extent) running the policy can be performed separately.
- Compute pool file and policy cache (write into `problem.pool` and `problem.policycache`):
```shell
./fast-downward.py domain.pddl problem.pddl \
--policy "pi=asnet_policy(domain_pddl=domain.pddl, problem_pddl=problem.pddl, snapshot=model.pkl)" \
--search "pool_fuzzer(max_steps=10000000, max_pool_size=200, policy=pi, eval=hmax(),just_write_policy_cache=true, policy_cache_file=problem.policycache, pool_file=problem.pool)"
```
- Compute dominance function and write it to file `problem.sim`:
```shell
./fast-downward.py domain.pddl problem.pddl \
--search "dummy_engine(testing_method=dummy_oracle(abs=builder_massim(merge_strategy=merge_dfp(), limit_transitions_merge=10000), write_sim_and_exit=true, sim_file=problem.sim, max_simulation_time=3600, max_total_time=14400))"
```
- Test policy with precomputed dominance function (in file `problem.sim`), precomputed pool (in file `problem.pool`) and precomputed policy cache (in file `problem.policycache`):
```shell
./fast-downward.py domain.pddl problem.pddl \
--search "pool_policy_tester(policy=cached_policy(), read_policy_cache=true, policy_cache_file=problem.policycache, pool_file=problem.pool, testing_method=iterative_improvement_oracle(read_simulation=true, sim_file=problem.sim))"
```