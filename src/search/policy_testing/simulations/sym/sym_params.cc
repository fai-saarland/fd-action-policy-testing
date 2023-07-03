#include "sym_params.h"

#include "../../../utils/timer.h"
#include "../simulations_manager.h"

#include <vector>

namespace simulations {
SymParamsMgr::SymParamsMgr(const options::Options &opts) :
    variable_ordering(opts.get<VariableOrderType>("var_order")),
    cudd_init_nodes(opts.get<long>("cudd_init_nodes")),
    cudd_init_cache_size(opts.get<long>("cudd_init_cache_size")),
    cudd_init_available_memory(opts.get<long>("cudd_init_available_memory")),
    max_tr_size(opts.get<int>("max_tr_size")),
    max_tr_time(opts.get<int>("max_tr_time")),
    mutex_type(opts.get<MutexType>("mutex_type")),
    max_mutex_size(opts.get<int>("max_mutex_size")),
    max_mutex_time(opts.get<int>("max_mutex_time")),
    max_pop_nodes(opts.get<int>("max_pop_nodes")),
    max_pop_time(opts.get<int>("max_pop_time")) {
    //Don't use edeletion with conditional effects
    if (mutex_type == MutexType::MUTEX_EDELETION && has_conditional_effects()) {
        std::cout << "Mutex type changed to mutex_and because the domain has conditional effects" << std::endl;
        mutex_type = MutexType::MUTEX_AND;
    }
}

SymParamsMgr::SymParamsMgr() :
    variable_ordering(REVERSE_LEVEL),
    cudd_init_nodes(16000000L),
    cudd_init_cache_size(16000000L),
    cudd_init_available_memory(0L),
    max_tr_size(100000),
    max_tr_time(60000),
    mutex_type(MutexType::MUTEX_EDELETION),
    max_mutex_size(100000),
    max_mutex_time(60000) {
    //Don't use edeletion with conditional effects
    if (mutex_type == MutexType::MUTEX_EDELETION && has_conditional_effects()) {
        std::cout << "Mutex type changed to mutex_and because the domain has conditional effects" << std::endl;
        mutex_type = MutexType::MUTEX_AND;
    }
}

void SymParamsMgr::print_options() const {
    std::cout << "CUDD Init: nodes=" << cudd_init_nodes <<
        " cache=" << cudd_init_cache_size <<
        " max_memory=" << cudd_init_available_memory << std::endl;
    std::cout << "TR(time=" << max_tr_time << ", nodes=" << max_tr_size << ")" << std::endl;
    std::cout << "Mutex(time=" << max_mutex_time << ", nodes=" << max_mutex_size << ", type=" << mutex_type << ")"
              << std::endl;
    std::cout << "Pop(time=" << max_pop_time << ", nodes=" << max_pop_nodes << ")" << std::endl;
}

void SymParamsMgr::add_options_to_parser(OptionParser &parser) {
    const std::vector<std::string> VariableOrderValues{
        "CG_GOAL_LEVEL", "CG_GOAL_RANDOM",
        "GOAL_CG_LEVEL", "RANDOM",
        "LEVEL", "REVERSE_LEVEL"};

    parser.add_enum_option<VariableOrderType>("var_order", VariableOrderValues,
                                              "variable ordering for the symbolic manager", "REVERSE_LEVEL");


    parser.add_option<long>("cudd_init_nodes", "Initial number of nodes in the cudd manager.",
                            "16000000L");

    parser.add_option<long>("cudd_init_cache_size",
                            "Initial number of cache entries in the cudd manager.", "16000000L");

    parser.add_option<long>("cudd_init_available_memory",
                            "Total available memory for the cudd manager.", "0L");

    parser.add_option<int>("max_tr_size", "maximum size of TR BDDs", "100000");

    parser.add_option<int>("max_tr_time",
                           "maximum time (ms) to generate TR BDDs", "60000");

    parser.add_enum_option<MutexType>("mutex_type", MutexTypeValues,
                                      "mutex type", "MUTEX_EDELETION");

    parser.add_option<int>("max_mutex_size",
                           "maximum size of mutex BDDs", "100000");

    parser.add_option<int>("max_mutex_time",
                           "maximum time (ms) to generate mutex BDDs", "60000");

    parser.add_option<int>("max_pop_nodes", "maximum size in pop operations", "1000000");
    parser.add_option<int>("max_pop_time", "maximum time (ms) in pop operations", "2000");
}


void SymParamsMgr::add_options_to_parser_simulation(OptionParser &parser) {
    parser.add_option<long>("cudd_init_nodes", "Initial number of nodes in the cudd manager.",
                            "16000000L");

    parser.add_option<long>("cudd_init_cache_size",
                            "Initial number of cache entries in the cudd manager.", "16000000L");

    parser.add_option<long>("cudd_init_available_memory",
                            "Total available memory for the cudd manager.", "0L");

    parser.add_option<int>("max_tr_size", "maximum size of TR BDDs", "1");

    parser.add_option<int>("max_tr_time",
                           "maximum time (ms) to generate TR BDDs", "1");

    parser.add_enum_option<MutexType>("mutex_type", MutexTypeValues,
                                      "mutex type", "MUTEX_AND");

    parser.add_option<int>("max_mutex_size",
                           "maximum size of mutex BDDs", "100000");

    parser.add_option<int>("max_mutex_time",
                           "maximum time (ms) to generate mutex BDDs", "10000");

    parser.add_option<int>("max_pop_nodes", "maximum size in pop operations", "1000000");
    parser.add_option<int>("max_pop_time", "maximum time (ms) in pop operations", "2000");

    const std::vector<std::string> VariableOrderValues{
        "CG_GOAL_LEVEL", "CG_GOAL_RANDOM",
        "GOAL_CG_LEVEL", "RANDOM",
        "LEVEL", "REVERSE_LEVEL"};

    parser.add_enum_option<VariableOrderType>("var_order", VariableOrderValues,
                                              "variable ordering for the symbolic manager", "REVERSE_LEVEL");
}


SymParamsSearch::SymParamsSearch(const Options &opts) :
    max_disj_nodes(opts.get<int>("max_disj_nodes")),
    min_estimation_time(opts.get<double>("min_estimation_time")),
    penalty_time_estimation_sum(opts.get<double>("penalty_time_estimation_sum")),
    penalty_time_estimation_mult(opts.get<double>("penalty_time_estimation_mult")),
    penalty_nodes_estimation_sum(opts.get<double>("penalty_time_estimation_sum")),
    penalty_nodes_estimation_mult(opts.get<double>("penalty_nodes_estimation_mult")),
    maxStepTime(opts.get<int>("max_step_time")),
    maxStepNodes(opts.get<int>("max_step_nodes")),
    maxStepNodesPerPlanningSecond(opts.get<int>("max_step_nodes_per_planning_second")),
    maxStepNodesMin(opts.get<int>("max_step_nodes_min")),
    maxStepNodesTimeStartIncrement(opts.get<int>("max_step_nodes_time_start_increment")),
    ratioUseful(opts.get<double>("ratio_useful")),
    minAllotedTime(opts.get<int>("min_alloted_time")),
    minAllotedNodes(opts.get<int>("min_alloted_nodes")),
    maxAllotedTime(opts.get<int>("max_alloted_time")),
    maxAllotedNodes(opts.get<int>("max_alloted_nodes")),
    ratioAllotedTime(opts.get<double>("ratio_alloted_time")),
    ratioAllotedNodes(opts.get<double>("ratio_alloted_nodes")),
    ratioAfterRelax(opts.get<double>("ratio_after_relax")),
    non_stop(opts.get<bool>("non_stop")),
    storeBDDsToDisk(opts.get<bool>("store_bdds_to_disk")),
    debug(opts.get<bool>("debug")) {
}

void SymParamsSearch::print_options() const {
    std::cout << "Disj(nodes=" << max_disj_nodes << ")" << std::endl;
    std::cout << "Estimation: min_time(" << min_estimation_time << ")" <<
        " time_penalty +(" << penalty_time_estimation_sum << ")" <<
        "*(" << penalty_time_estimation_mult << ")" <<
        " nodes_penalty +(" << penalty_nodes_estimation_sum << ")" <<
        "*(" << penalty_nodes_estimation_mult << ")" << std::endl;
    std::cout << "MaxStep(time=" << maxStepTime << ", nodes=" << maxStepNodes << ", nodes_per_planning_second="
              << maxStepNodesPerPlanningSecond << ")" << std::endl;
    std::cout << "Ratio useful: " << ratioUseful << std::endl;
    std::cout << "   Min alloted time: " << minAllotedTime << " nodes: " << minAllotedNodes << std::endl;
    std::cout << "   Max alloted time: " << maxAllotedTime << " nodes: " << maxAllotedNodes << std::endl;
    std::cout << "   Mult alloted time: " << ratioAllotedTime << " nodes: " << ratioAllotedNodes << std::endl;
    std::cout << "   Ratio after relax: " << ratioAfterRelax << std::endl;
}

void SymParamsSearch::add_options_to_parser(OptionParser &parser, int maxStepTime, int maxStepNodes) {
    parser.add_option<int>("max_disj_nodes",
                           "maximum size to enforce disjunction before image",
                           std::to_string(std::numeric_limits<int>::max()));

    parser.add_option<double>("min_estimation_time",
                              "minimum time to perform linear interpolation for estimation", "1000");

    parser.add_option<double>("penalty_time_estimation_sum",
                              "time added when violated alloted time", "1000");
    parser.add_option<double>("penalty_time_estimation_mult",
                              "multiplication factor when violated alloted time", "2");

    parser.add_option<double>("penalty_nodes_estimation_sum",
                              "nodes added when violated alloted nodes", "1000");
    parser.add_option<double>("penalty_nodes_estimation_mult",
                              "multiplication factor when violated alloted nodes", "2");

    parser.add_option<int>("max_step_time", "allowed time to perform a step in the search",
                           std::to_string(maxStepTime));
    parser.add_option<int>("max_step_nodes", "allowed nodes to perform a step in the search",
                           std::to_string(maxStepNodes));

    parser.add_option<int>("max_step_nodes_per_planning_second",
                           "allowed nodes to perform a step in the search. Starts at 0 and increases by x per second.",
                           "100");

    parser.add_option<int>("max_step_nodes_min", "allowed nodes to perform a step in the search. minimum value.",
                           "10000");

    parser.add_option<int>("max_step_nodes_time_start_increment", "max_step_nodes_min until this time",
                           "300");


    parser.add_option<double>("ratio_useful",
                              "Percentage of nodes that can potentially prune in the frontier for an heuristic to be useful",
                              "0.0");

    //The default value is a 50% percent more than maxStepTime,
    parser.add_option<int>("min_alloted_time",
                           "minimum alloted time for an step", std::to_string(60e3));
    parser.add_option<int>("min_alloted_nodes",
                           "minimum alloted nodes for an step", std::to_string(10e6));

    parser.add_option<int>("max_alloted_time",
                           "maximum alloted time for an step", std::to_string(60e3));
    parser.add_option<int>("max_alloted_nodes",
                           "maximum alloted nodes for an step", std::to_string(15e6));

    parser.add_option<double>("ratio_alloted_time",
                              "multiplier to decide alloted time for a step", "2.0");
    parser.add_option<double>("ratio_alloted_nodes",
                              "multiplier to decide alloted nodes for a step", "2.0");
    parser.add_option<double>("ratio_after_relax",
                              "multiplier to decide alloted nodes for a step", "0.8");

    parser.add_option<bool>("non_stop",
                            "Removes initial state from closed to avoid backward search to stop.",
                            "false");

    parser.add_option<bool>("store_bdds_to_disk",
                            "Saves BDDs to disk.",
                            "false");


    parser.add_option<bool>("debug",
                            "print debug trace",
                            "false");
}

int SymParamsSearch::getMaxStepNodes() const {
    if (utils::g_timer() < maxStepNodesTimeStartIncrement)
        return maxStepNodesMin;
    else
        return std::min<double>(maxStepNodes,
                                maxStepNodesMin + maxStepNodesPerPlanningSecond *
                                (utils::g_timer() - maxStepNodesTimeStartIncrement));
}
}
