#include "command_line.h"
#include "search_algorithm.h"

#include "tasks/root_task.h"
#include "task_utils/task_properties.h"
#include "utils/logging.h"
#include "utils/system.h"
#include "utils/timer.h"

#ifdef POLICY_TESTING_ENABLED
#include "policy_testing/policies/remote_policy.h"
#include <sstream>
#endif /* POLICY_TESTING_ENABLED */

#include <iostream>

using namespace std;
using utils::ExitCode;

int main(int argc, const char **argv) {
    utils::register_event_handlers();
    string input_file_override; // if set, read input from this file instead of from stdin

#ifdef POLICY_TESTING_ENABLED
    if (argc >= 3 && static_cast<string>(argv[1]) == "--remote-policy") {
        try {
            policy_testing::RemotePolicy::establish_connection(static_cast<string>(argv[2]));
        } catch (const policy_testing::RemotePolicyError &err) {
            err.print();
            utils::exit_with(ExitCode::REMOTE_POLICY_ERROR);
        }
        for (int i = 3; i < argc; ++i) {
            argv[i - 2] = argv[i];
        }
        argc -= 2;
    } else if (argc >= 3 && static_cast<string>(argv[1]) == "--input-file") {
        input_file_override = static_cast<string>(argv[2]);
        for (int i = 3; i < argc; ++i) {
            argv[i - 2] = argv[i];
        }
        argc -= 2;
    }
#endif /* POLICY_TESTING_ENABLED */

    if (argc < 2) {
        utils::g_log << usage(argv[0]) << endl;
        utils::exit_with(ExitCode::SEARCH_INPUT_ERROR);
    }

    bool unit_cost = false;
    if (static_cast<string>(argv[1]) != "--help") {
        utils::g_log << "reading input..." << endl;
#ifdef POLICY_TESTING_ENABLED
        if (!policy_testing::RemotePolicy::connection_established()) {
            if (input_file_override.empty()) {
                tasks::read_root_task(cin);
            } else {
                std::ifstream file_stream(input_file_override);
                if (!file_stream.is_open()) {
                    std::cerr << "Cannot open " << input_file_override << std::endl;
                }
                tasks::read_root_task(file_stream);
            }
        } else {
            try {
                std::string task = policy_testing::RemotePolicy::input_fdr();
                std::istringstream strin(task);
                tasks::read_root_task(strin);
            } catch (const policy_testing::RemotePolicyError &err) {
                err.print();
                utils::exit_with(ExitCode::REMOTE_POLICY_ERROR);
            }
        }
#else /* POLICY_TESTING_ENABLED */
        tasks::read_root_task(cin);
#endif /* POLICY_TESTING_ENABLED */
        utils::g_log << "done reading input!" << endl;
        TaskProxy task_proxy(*tasks::g_root_task);
        unit_cost = task_properties::is_unit_cost(task_proxy);
    }

    shared_ptr<SearchAlgorithm> search_algorithm =
        parse_cmd_line(argc, argv, unit_cost);


    utils::Timer search_timer;
    search_algorithm->search();
    search_timer.stop();
    utils::g_timer.stop();

    search_algorithm->save_plan_if_necessary();
    search_algorithm->print_statistics();
    utils::g_log << "Search time: " << search_timer << endl;
    utils::g_log << "Total time: " << utils::g_timer << endl;

    ExitCode exitcode = search_algorithm->found_solution()
        ? ExitCode::SUCCESS
        : ExitCode::SEARCH_UNSOLVED_INCOMPLETE;
    exit_with(exitcode);
}
