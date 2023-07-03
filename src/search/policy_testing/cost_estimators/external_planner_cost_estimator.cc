#include "external_planner_cost_estimator.h"

#include "../../option_parser.h"
#include "../../plugin.h"


#include <algorithm>
#include <istream>
#include <memory>
#include <string>
#include <unistd.h>

namespace policy_testing {
ExternalPlannerPlanCostEstimator::ExternalPlannerPlanCostEstimator(const options::Options &opts)
    : PlanCostEstimator()
      , plan_file_parser_(nullptr)
      , downward_path_(opts.get<std::string>("downward_path"))
      , preprocess_path_(
          opts.contains("preprocess_path")
              ? opts.get<std::string>("preprocess_path")
              : "")
      // , plan_found_exit_code_(opts.get<int>("solved"))
      , unsolvable_exit_code_(opts.get<int>("unsolvable")) {
    std::ostringstream params;
    bool sep = false;
    for (const auto &p : opts.get_list<std::string>("params")) {
        params << (sep ? " " : "") << "\"" << p << "\"";
        sep = true;
    }
    params_ = params.str();
}

void
ExternalPlannerPlanCostEstimator::initialize() {
    if (initialized) {
        return;
    }
    plan_file_parser_ = std::make_unique<PlanFileParser>(get_task_proxy());
    PlanCostEstimator::initialize();
}

void
ExternalPlannerPlanCostEstimator::add_options_to_parser(options::OptionParser &parser) {
    parser.add_option<std::string>("downward_path");
    // parser.add_option<int>("solved");
    parser.add_option<int>("unsolvable", "", "-1");
    parser.add_option<std::string>(
        "preprocess_path", "", options::OptionParser::NONE);
    parser.add_list_option<std::string>("params");
}

int
ExternalPlannerPlanCostEstimator::run_planner(
    const State &s,
    std::vector<OperatorID> &plan) {
#ifdef LINUX
    std::ofstream output;
    output.open("ext_output.sas");
    output << get_modified_initial_state_task(get_task(), s)->get_sas();
    output.close();

    std::string of = "ext_output.sas";
    if (!preprocess_path_.empty()) {
        of = "output";
        std::string cmd = preprocess_path_ + " < ext_output.sas";
        [[maybe_unused]] const int call_res = system(cmd.c_str());
    }

    std::string cmd = downward_path_ + " " + params_ + " < " + of;
    const int downward_call_res = system(cmd.c_str());
    int rescode = DEAD_END;

    if (downward_call_res != unsolvable_exit_code_) {
        rescode = UNKNOWN;

        std::vector<OperatorID> temp;
        bool res = load("sas_plan", temp);
        if (res) {
            plan.swap(temp);
            rescode = 0;
        }
        for (int i = 1;; ++i) {
            std::ostringstream fname;
            fname << "sas_plan." << i;
            res = load(fname.str(), temp);
            if (res) {
                rescode = 0;
                temp.swap(plan);
            } else {
                break;
            }
        }
    }
    cleanup();
#ifndef NDEBUG
    if (rescode == 0) {
        assert(verify_plan(get_task(), s, plan));
    }
#endif
    return rescode;
#elif
#error "At the moment this does not work under other operating systems"
#endif
}

bool
ExternalPlannerPlanCostEstimator::load(
    const std::string &file_name,
    std::vector<OperatorID> &plan) {
    std::ifstream aras_output(file_name);
    if (!aras_output.good()) {
        return false;
    }
    plan.clear();
    if (!plan_file_parser_->parse(aras_output, plan)) {
        utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
    }
    aras_output.close();
    unlink(file_name.c_str());
    return true;
}

void
ExternalPlannerPlanCostEstimator::cleanup() {
    // Do some cleanup that ExternalPlannerOracle misses to do
    unlink("ext_output.sas");
    unlink("output");
    unlink("elapsed.time");
}

int
ExternalPlannerPlanCostEstimator::compute_value(const State &state) {
    std::vector<OperatorID> plan;
    const int code = run_planner(state, plan);
    if (code == 0) {
        return calculate_plan_cost(get_task(), plan);
    } else if (code == DEAD_END) {
        return DEAD_END;
    }
    return UNKNOWN;
}

static Plugin<PlanCostEstimator> _plugin(
    "external_planner_plan_cost_estimator",
    options::parse<PlanCostEstimator, ExternalPlannerPlanCostEstimator>);
} // namespace policy_testing
