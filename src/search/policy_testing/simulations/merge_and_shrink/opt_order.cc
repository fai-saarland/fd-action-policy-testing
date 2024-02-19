#include "opt_order.h"

#include <iostream>

#include "../../../task_utils/causal_graph.h"

namespace simulations {
//Returns an optimized variable ordering that reorders the variables
//according to the standard causal graph criterion
/*
void InfluenceGraph::compute_gamer_ordering(std::vector<int> &var_order) {
    const int num_variables = global_simulation_task()->get_num_variables();
    if (var_order.empty()) {
        for (int v = 0; v < num_variables; v++) {
            var_order.push_back(v);
        }
    }

    InfluenceGraph ig_partitions(num_variables);
    const auto &causal_graph = global_simulation_task_proxy()->get_causal_graph();

    for (int v = 0; v < num_variables; v++) {
        for (int v2: causal_graph.get_successors(v)) {
            if (v != v2) {
                ig_partitions.set_influence(v, v2);
            }
        }
    }

    ig_partitions.get_ordering(var_order);
}
 */


void InfluenceGraph::get_ordering(std::vector<int> &ordering) const {
    long value_optimization_function = optimize_variable_ordering_gamer(ordering, 50000);
    std::cout << "Value: " << value_optimization_function << std::endl;

    for (int counter = 0; counter < 20; counter++) {
        std::vector<int> new_order;
        randomize(ordering, new_order);     //Copy the order randomly
        long new_value = optimize_variable_ordering_gamer(new_order, 50000);

        if (new_value < value_optimization_function) {
            value_optimization_function = new_value;
            ordering.swap(new_order);
            std::cout << "New value: " << value_optimization_function << std::endl;
        }
    }
}


void InfluenceGraph::randomize(std::vector<int> &ordering, std::vector<int> &new_order) {
    for (int i = 0; i < ordering.size(); i++) {
        int rnd_pos = order_rng.random(ordering.size() - i);
        int pos = -1;
        do {
            pos++;
            bool found;
            do {
                found = false;
                for (int j : new_order) {
                    if (j == ordering[pos]) {
                        found = true;
                        break;
                    }
                }
                if (found)
                    pos++;
            } while (found);
        } while (rnd_pos-- > 0);
        new_order.push_back(ordering[pos]);
    }
}


long InfluenceGraph::optimize_variable_ordering_gamer(std::vector<int> &order,
                                                      int iterations) const {
    long totalDistance = compute_function(order);

    long oldTotalDistance = totalDistance;
    //Repeat iterations times
    for (int counter = 0; counter < iterations; counter++) {
        //Swap variable
        int swapIndex1 = order_rng.random(order.size());
        int swapIndex2 = order_rng.random(order.size());
        if (swapIndex1 == swapIndex2)
            continue;

        //Compute the new value of the optimization function
        for (int i = 0; i < order.size(); i++) {
            if (i == swapIndex1 || i == swapIndex2)
                continue;

            if (influence(order[i], order[swapIndex1]))
                totalDistance += (-(i - swapIndex1) * (i - swapIndex1)
                                  + (i - swapIndex2) * (i - swapIndex2));

            if (influence(order[i], order[swapIndex2]))
                totalDistance += (-(i - swapIndex2) * (i - swapIndex2)
                                  + (i - swapIndex1) * (i - swapIndex1));
        }

        //Apply the swap if it is worthy
        if (totalDistance < oldTotalDistance) {
            int tmp = order[swapIndex1];
            order[swapIndex1] = order[swapIndex2];
            order[swapIndex2] = tmp;
            oldTotalDistance = totalDistance;
        } else {
            totalDistance = oldTotalDistance;
        }
    }
    //  cout << "Total distance: " << totalDistance << endl;
    return totalDistance;
}


long InfluenceGraph::compute_function(const std::vector<int> &order) const {
    long totalDistance = 0;
    for (int i = 0; i < order.size() - 1; i++) {
        for (int j = i + 1; j < order.size(); j++) {
            if (influence(order[i], order[j])) {
                totalDistance += (i - j) * (i - j);
            }
        }
    }
    return totalDistance;
}


InfluenceGraph::InfluenceGraph(int num) {
    influence_graph.resize(num);
    for (auto &i: influence_graph) {
        i.resize(num, 0);
    }
}


void InfluenceGraph::optimize_variable_ordering_gamer(std::vector<int> &order,
                                                      std::vector<int> &partition_begin,
                                                      std::vector<int> &partition_sizes,
                                                      int iterations) const {
    long totalDistance = compute_function(order);

    long oldTotalDistance = totalDistance;
    //Repeat iterations times
    for (int counter = 0; counter < iterations; counter++) {
        //Swap variable
        int partition = order_rng.random(partition_begin.size());
        if (partition_sizes[partition] <= 1)
            continue;
        int swapIndex1 = partition_begin[partition] + order_rng.random(partition_sizes[partition]);
        int swapIndex2 = partition_begin[partition] + order_rng.random(partition_sizes[partition]);
        if (swapIndex1 == swapIndex2)
            continue;

        //Compute the new value of the optimization function
        for (int i = 0; i < order.size(); i++) {
            if (i == swapIndex1 || i == swapIndex2)
                continue;

            if (influence(order[i], order[swapIndex1]))
                totalDistance += (-(i - swapIndex1) * (i - swapIndex1)
                                  + (i - swapIndex2) * (i - swapIndex2));

            if (influence(order[i], order[swapIndex2]))
                totalDistance += (-(i - swapIndex2) * (i - swapIndex2)
                                  + (i - swapIndex1) * (i - swapIndex1));
        }

        //Apply the swap if it is worthy
        if (totalDistance < oldTotalDistance) {
            int tmp = order[swapIndex1];
            order[swapIndex1] = order[swapIndex2];
            order[swapIndex2] = tmp;
            oldTotalDistance = totalDistance;
        } else {
            totalDistance = oldTotalDistance;
        }
    }
}
}
