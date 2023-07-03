#include "utilities.h"

#include <cassert>
#include <csignal>
#include <iostream>
#include <fstream>
#include <limits>
//#include "globals.h"
//#include "timer.h"

#include "../../../utils/timer.h"

namespace simulations {
#if OPERATING_SYSTEM == LINUX

static void exit_handler(int exit_code, void *hint);

#elif OPERATING_SYSTEM == OSX
static void exit_handler();
#include <mach/mach.h>
#elif OPERATING_SYSTEM == CYGWIN
// nothing
#endif

static char *memory_padding = new char[512 * 1024];

static void out_of_memory_handler();

static void signal_handler(int signal_number);


void register_event_handlers() {
    // When running out of memory, release some emergency memory and
    // terminate.
    std::set_new_handler(out_of_memory_handler);

    // On exit or when receiving certain signals such as SIGINT (Ctrl-C),
    // print the peak memory usage.
#if OPERATING_SYSTEM == LINUX
    on_exit(exit_handler, nullptr);
#elif OPERATING_SYSTEM == OSX
    atexit(exit_handler);
#elif OPERATING_SYSTEM == CYGWIN
    // nothing
#endif
    signal(SIGABRT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGSEGV, signal_handler);
    signal(SIGINT, signal_handler);
}

#if OPERATING_SYSTEM != CYGWIN
#if OPERATING_SYSTEM == LINUX

void exit_handler(int, void *) {
#elif OPERATING_SYSTEM == OSX
void exit_handler() {
#endif
    //print_peak_memory();
}

#endif

void exit_with(ExitCode exitcode) {
    print_peak_memory();
    switch (exitcode) {
    case EXIT_PLAN_FOUND:
        std::cout << "Solution found." << std::endl;
        std::cout << "solvable" << std::endl;
        break;
    case EXIT_CRITICAL_ERROR:
        std::cerr << "Unexplained error occurred." << std::endl;
        break;
    case EXIT_INPUT_ERROR:
        std::cerr << "Usage error occurred." << std::endl;
        break;
    case EXIT_UNSUPPORTED:
        std::cerr << "Tried to use unsupported feature." << std::endl;
        break;
    case EXIT_UNSOLVABLE:
        std::cout << "Total time: " << utils::g_timer << std::endl;
        std::cout << "Task is provably unsolvable." << std::endl;
        std::cout << "unsolvable" << std::endl;
        break;
    case EXIT_UNSOLVED_INCOMPLETE:
        std::cout << "Search stopped without finding a solution." << std::endl;
        break;
    case EXIT_OUT_OF_MEMORY:
        std::cout << "Memory limit has been reached." << std::endl;
        break;
    case EXIT_TIMEOUT:
        std::cout << "Time limit has been reached." << std::endl;
        break;
    default:
        std::cerr << "Exitcode: " << exitcode << std::endl;
        ABORT("Unkown exitcode.");
    }
    exit(exitcode);
}

static void out_of_memory_handler() {
    assert(memory_padding);
    delete[] memory_padding;
    memory_padding = nullptr;
    std::cout << "Failed to allocate memory. Released memory buffer." << std::endl;
    exit_with(EXIT_OUT_OF_MEMORY);
}

void signal_handler(int signal_number) {
    // See glibc manual: "Handlers That Terminate the Process"
    static volatile sig_atomic_t handler_in_progress = 0;
    if (handler_in_progress)
        raise(signal_number);
    handler_in_progress = 1;
    print_peak_memory();
    std::cout << "caught signal " << signal_number << " -- exiting" << std::endl;
    signal(signal_number, SIG_DFL);
    raise(signal_number);
}

int get_peak_memory_in_kb() {
    // On error, produces a warning on cerr and returns -1.
    int memory_in_kb = -1;

#if OPERATING_SYSTEM == OSX
    // Based on http://stackoverflow.com/questions/63166/how-to-determine-cpu-and-memory-consumption-from-inside-a-process
    task_basic_info t_info;
    mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

    if (task_info(mach_task_self(), TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&t_info),
                  &t_info_count) == KERN_SUCCESS)
        memory_in_kb = t_info.virtual_size / 1024;
#else
    std::ifstream procfile("/proc/self/status");
    std::string word;
    while (procfile.good()) {
        procfile >> word;
        if (word == "VmPeak:") {
            procfile >> memory_in_kb;
            break;
        }
        // Skip to end of line.
        procfile.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
    if (procfile.fail())
        memory_in_kb = -1;
#endif

    if (memory_in_kb == -1)
        std::cerr << "warning: could not determine peak memory" << std::endl;
    return memory_in_kb;
}

void print_peak_memory() {
    std::cout << "Peak memory: " << get_peak_memory_in_kb() << " KB" << std::endl;
}
}
