#include "variable_partition_finder.h"
#include <iostream>

#include "../simulations_manager.h"

namespace simulations {
void VariablePartitionFinder::dump() const {
    std::cout << "Partition: " << std::endl;
    for (auto &p: partitions) {
        int size = 1;
        for (int v: p) {
            std::cout << " " << v << "(" << global_simulation_task()->get_fact_name(FactPair(v, 0)) << ")";
            size *= global_simulation_task()->get_variable_domain_size(v);
        }
        std::cout << " (" << size << ")" << std::endl;
    }
    std::cout << std::endl;
}

void VariablePartitionGreedy::init() {
    std::cout << "Init" << std::endl;

    const int num_variables = global_simulation_task()->get_num_variables();
    partitions.resize(num_variables);
    for (int i = 0; i < num_variables; i++) {
        partitions[i].push_back(i);
    }
    std::cout << "Copy g_var domain" << std::endl;

    const int num_operators = global_simulation_task()->get_num_operators();
    std::vector<int> domain_sizes(num_operators);
    for (int i = 0; i < num_operators; ++i) {
        domain_sizes[i] = global_simulation_task()->get_variable_domain_size(i);
    }
    part_size = domain_sizes;

    //Set weights according to the causal graph
    for (int op = 0; op < num_operators; op++) {
        std::set<int> pre_vars, eff_vars;
        get_vars(op, pre_vars, eff_vars);
        for (int v: eff_vars) {
            for (int v2: pre_vars) {
                weights[v][v2].insert(op);
                weights[v2][v].insert(op);
            }
            for (int v2: eff_vars) {
                weights[v][v2].insert(op);
                weights[v2][v].insert(op);
            }
        }
    }
}

void VariablePartitionGreedy::merge(int p1, int p2) {
    part_size.push_back(part_size[p1] * part_size[p2]);
    part_size[p1] = 0;
    part_size[p2] = 0;
    int newp = partitions.size();
    partitions.push_back(partitions[p1]);
    partitions.back().insert(partitions.back().end(),
                             std::begin(partitions[p2]),
                             std::end(partitions[p2]));
    for (int i = 0; i < partitions.size(); i++) {
        std::set_union(std::begin(weights[p1][i]), std::end(weights[p1][i]),
                       std::begin(weights[p2][i]), std::end(weights[p2][i]),
                       inserter(weights[newp][i], std::end(weights[newp][i])));


        std::set_union(std::begin(weights[i][p1]), std::end(weights[i][p1]),
                       std::begin(weights[i][p2]), std::end(weights[i][p2]),
                       inserter(weights[i][newp], std::end(weights[i][newp])));
    }
}


void VariablePartitionGreedy::find() {
    std::cout << "Find greedy" << std::endl;
    init();
    while (true) {
        //Greedily pick two partitions
        std::pair<int, int> parts = pick_parts();
        std::cout << "Picked: " << parts.first << " " << parts.second << std::endl;
        if (parts.first == -1)
            break;
        merge(parts.first, parts.second);     //Merge both partitions
    }

    std::cout << "Find greedy loop done" << std::endl;
    std::vector<std::vector<int>> aux;
    for (int i = 0; i < partitions.size(); ++i) {
        if (part_size[i])
            aux.push_back(partitions[i]);
    }
    partitions.swap(aux);
    std::cout << "Find greedy done: " << partitions.size() << " " << aux.size() << std::endl;
}


std::pair<int, int> VariablePartitionGreedy::pick_parts() const {
    auto best = std::pair<int, int> {-1, -1};
    int best_weight = -1;
    for (int i = 0; i < partitions.size(); ++i) {
        if (!part_size[i])
            continue;
        for (int j = i + 1; j < partitions.size(); ++j) {
            if (!part_size[j])
                continue;
            int w = 0;
            if (weights.count(i) && weights.at(i).count(j)) {
                weights.at(i).at(j).size();
            }
            //cout << w << " " << i << " " << j << endl;
            if (part_size[i] * part_size[j] < limit_size &&
                w > best_weight) {
                best_weight = w;
                best = std::pair<int, int> {i, j};
            }
        }
    }
    return best;
}
}
