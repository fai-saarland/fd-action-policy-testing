#! /usr/bin/env python
# -*- coding: utf-8 -*-

import common_setup

import os

from lab.environments import LocalEnvironment, BaselSlurmEnvironment

REVISIONS = [
    "main",
]

CONFIGS = [
    common_setup.IssueConfig("lama-first", [],
                             driver_options=["--alias", "lama-first"]),
    common_setup.IssueConfig("seq-opt-bjolp", [],
                             driver_options=["--alias", "seq-opt-bjolp"]),
]

BENCHMARKS_DIR = os.environ["DOWNWARD_BENCHMARKS"]
REPO = os.environ["DOWNWARD_REPO"]

if common_setup.is_running_on_cluster():
    SUITE = common_setup.DEFAULT_SATISFICING_SUITE
    ENVIRONMENT = BaselSlurmEnvironment(
        partition="infai_2",
        email="clemens.buechner@unibas.ch",
        export=["PATH", "DOWNWARD_BENCHMARKS"],
    )
else:
    SUITE = common_setup.IssueExperiment.DEFAULT_TEST_SUITE
    ENVIRONMENT = LocalEnvironment(processes=2)

exp = common_setup.IssueExperiment(
    revisions=REVISIONS,
    configs=CONFIGS,
    environment=ENVIRONMENT,
)

exp.add_suite(BENCHMARKS_DIR, SUITE)

exp.add_parser(exp.ANYTIME_SEARCH_PARSER)
exp.add_parser(exp.EXITCODE_PARSER)
exp.add_parser(exp.PLANNER_PARSER)
exp.add_parser(exp.SINGLE_SEARCH_PARSER)

exp.add_step("build", exp.build)
exp.add_step("start", exp.start_runs)
exp.add_fetcher(name="fetch")
exp.add_absolute_report_step()
exp.add_parse_again_step()

exp.run_steps()

