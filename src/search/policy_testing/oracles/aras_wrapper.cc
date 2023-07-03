#include "aras_wrapper.h"

#include "../utils.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <unistd.h>

namespace policy_testing {
ArasWrapper::ArasWrapper(
    std::string path,
    std::shared_ptr<AbstractTask> task,
    TaskProxy &task_proxy)
    : aras_directory(std::move(path))
      , task(std::move(task))
      , plan_file_parser(task_proxy) {
}

void
ArasWrapper::prepare_aras_input(
    const State &state,
    const std::vector<OperatorID> &plan) {
    std::ofstream aras_input("aras_sas_plan_input");
    for (OperatorID op_id : plan) {
        aras_input << "(" << task->get_operator_name(op_id.get_index(), false)
                   << ")" << std::endl;
    }
    aras_input.close();

    std::shared_ptr<AbstractTask> modified_task =
        get_modified_initial_state_task(task, state);
    std::ofstream output;
    output.open("aras_output.sas");
    output << modified_task->get_sas();
    output.close();
}

void
ArasWrapper::call_aras(int time_limit) {
    std::cout.flush();
#ifdef LINUX
    std::string command =
        aras_directory + "/src/preprocess/preprocess < aras_output.sas  >>/dev/null 2>>/dev/null";
    [[maybe_unused]] const int preprocess_ret_code = system(command.c_str());
    std::string command_downward = aras_directory
        + "/src/search/downward"
        " --postprocessor \"aras(reg_graph=false, memory_limit=1000000, "
        "time_limit="
        + std::to_string(time_limit)
        + ")\""
        " --input-plan-file aras_sas_plan_input"
        " --plan-file aras_sas_plan_output"
        " < output >>/dev/null 2>>/dev/null";
    [[maybe_unused]] const int aras_ret_code = system(command_downward.c_str());
#elif
#error "At the moment this does not work under other operating systems"
#endif
}

bool
ArasWrapper::load(const std::string &file_name, std::vector<OperatorID> &plan) {
    std::ifstream aras_output(file_name);
    if (!aras_output.good()) {
        return false;
    }
    if (!plan_file_parser.parse(aras_output, plan)) {
        utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
    }
    aras_output.close();
    unlink(file_name.c_str());
    return true;
}

bool
ArasWrapper::improve_plan(
    int time_limit,
    const State &state,
    std::vector<OperatorID> &plan) {
    prepare_aras_input(state, plan);
    call_aras(time_limit);

    plan.clear();

    std::vector<OperatorID> temp;
    int best_cost = 0;

    bool found = load("aras_sas_plan_output", plan);
    if (found) {
        best_cost = calculate_plan_cost(task, plan);
    }

    for (int i = 1;; ++i) {
        std::ostringstream fname;
        fname << "aras_sas_plan_output." << i;
        bool res = load(fname.str(), temp);
        if (res) {
            const int cost = calculate_plan_cost(task, temp);
            if (!found || cost < best_cost) {
                best_cost = cost;
                plan.swap(temp);
                temp.clear();
                found = true;
            }
        } else {
            break;
        }
    }

    return found;
}

[[maybe_unused]] void
ArasWrapper::cleanup() {
    // Do some cleanup that Aras misses to do
    unlink("aras_output.sas");
    unlink("output");
    unlink("elapsed.time");
    unlink("aras_sas_plan_input");
    unlink("plan_numbers_and_cost");
}
} // namespace policy_testing
