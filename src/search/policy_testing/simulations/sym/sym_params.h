#pragma once

#include "sym_enums.h"
#include "../../../option_parser.h"

//TODO: move variable_order_finder somewhere else
#include "../merge_and_shrink/variable_order_finder.h"

namespace simulations {
//Parameters are divided in two classes. SymParamsMgr for mgr params
//and SymParamsSearch for parameters that control the characteristics
//of a search.

class SymParamsMgr {
public:
    //TODO: Rename merge strategy type to VarOrder

    // Var order used by the algorithm. By default: MERGE_LINEAR_LEVEL
    // so that we follow the preprocessor order (which should be
    // optimized)
    VariableOrderType variable_ordering;
    //Parameters to initialize the CUDD manager
    long cudd_init_nodes;     //Number of initial nodes
    long cudd_init_cache_size;     //Initial cache size
    long cudd_init_available_memory;     //Maximum available memory (bytes)

    //Parameters to generate the TRs
    int max_tr_size;
    int max_tr_time;

    //Parameters to generate the mutex BDDs
    MutexType mutex_type;
    int max_mutex_size;
    int max_mutex_time;

    //Time and memory bounds for auxiliary operations
    int max_pop_nodes;
    int max_pop_time;


    SymParamsMgr();

    SymParamsMgr(const options::Options &opts);

    static void add_options_to_parser(OptionParser &parser);

    static void add_options_to_parser_simulation(OptionParser &parser);

    void print_options() const;
};

class SymParamsSearch {
public:
    //By default max<int>. If lower, it allows for skip disjunction if the size of S is greater.
    int max_disj_nodes;

    //Parameters for sym_step_cost_estimation
    double min_estimation_time;             // Don't estimate if time is lower
    double penalty_time_estimation_sum;     // violated_time = sum + time*mult
    double penalty_time_estimation_mult;
    double penalty_nodes_estimation_sum;    // violated_nodes = sum + nodes*mult
    double penalty_nodes_estimation_mult;

    //Parameters to control isUseful() and isSearchable()
    int maxStepTime, maxStepNodes;

    //Allows to scale maxStepNodes with planning time (starts at 100*x
    //during 100s and then grows at a rate of 1x)
    int maxStepNodesPerPlanningSecond, maxStepNodesMin, maxStepNodesTimeStartIncrement;
    double ratioUseful;

    // Parameters to decide the allotted time for a step
    // alloted = max(minAlloted, estimated*multAlloted)
    int minAllotedTime, minAllotedNodes;       // min allotted time and nodes to a step
    int maxAllotedTime, maxAllotedNodes;       // min allotted time and nodes to a step
    double ratioAllotedTime, ratioAllotedNodes;     // factor to multiply the estimation

    double ratioAfterRelax;

    bool non_stop;
    bool storeBDDsToDisk;

    bool debug;

    SymParamsSearch(const Options &opts);

    static void add_options_to_parser(OptionParser &parser, int maxStepTime, int maxStepNodes);

    //Parameters with default values for hierarchy policies
    static void add_options_to_parser_abstractions(OptionParser &parser, int maxStepTime, int maxStepNodes);

    void print_options() const;

    inline double getAllotedTime(double estimatedTime) const {
        return std::min(maxAllotedTime,
                        std::max<int>(estimatedTime * ratioAllotedTime, minAllotedTime));
    }

    inline double getAllotedNodes(double estimatedNodes) const {
        return std::min(maxAllotedNodes, std::max<int>(estimatedNodes * ratioAllotedNodes, minAllotedNodes));
    }

    void inheritParentParams(const SymParamsSearch &other) {
        maxStepTime = std::min(maxStepTime, other.maxStepTime);
        maxStepNodes = std::min(maxStepNodes, other.maxStepNodes);
    }

    int getMaxStepNodes() const;

    inline bool get_non_stop() const {
        return non_stop;
    }

    inline bool getStoreBDDsToDisk() const {
        return storeBDDsToDisk;
    }
};
}
