# Automatic Metamorphic Test Oracles for Action-Policy Testing

This repository contains the source code for action-policy testing based on the paper "Automatic Metamorphic Test Oracles for Action-Policy Testing".

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

Please build the project as a part of the [the bughive framework](https://github.com/fai-saarland/bughive/) using the Makefile provided there. The Makefile target is `fd-action-policy-testing`.

You need to have [boost](https://www.boost.org/) installed. The minimum required version is `1.74`.

## Code

The related to policy testing is primarily located in `src/search/policy_testing/`. 
The source code for the computation of dominance functions can be found in `src/search/policy_testing/simulations/`.
We made slight modifications to the code base since we ran the experiments for the paper, e.g., to make sure one can compile the source code using more recent compilers.

## Usage

Check out the test drivers in [the bughive repository](https://github.com/fai-saarland/bughive/tree/master/test_drivers) to learn how to easily invoke the tool. 

A possible search configuration could be `pool_fuzzer(testing_method=<oracle>)`, where `<oracle>` is one of the following:

* `aras(...)`
* `atomic_unrelaxation_oracle(...)`
* `bounded_lookahead_oracle(...)`
* `composite_oracle(...)`
* `dummy_oracle(...)`
* `estimator_based_oracle(...)`
* `iterative_improvement_oracle(...)`
* `unrelaxation_oracle(...)`

`iterative_improvement_oracle` implements the bound maintenance oracles (BMOs), while `atomic_unrelaxation_oracle` and `unrelaxation_oracle` implement state morphing oracles (SMOs).

You can learn about the options supported by an oracle (say `unrelaxation_oracle`) by calling `./fast-downward.py --search -- --help unrelaxation_oracle`.

