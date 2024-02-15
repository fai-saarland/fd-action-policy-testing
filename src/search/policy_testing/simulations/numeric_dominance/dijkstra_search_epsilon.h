#pragma once

#include <vector>
#include "../utils/priority_queue.h"
#include "int_epsilon.h"

namespace simulations {
//Copied from abstraction.cc (move somewhere else?)
template<typename T, typename Queue>
void dijkstra_search_epsilon(
    const std::vector<std::vector<std::pair<int, T>>> &graph,
    Queue &queue,
    std::vector<T> &distances,
    std::vector<int> *states_reached) {
    while (!queue.empty()) {
        std::pair<T, int> top_pair = queue.pop();
        T distance = top_pair.first;
        int state = top_pair.second;
        T state_distance = distances[state];
        if (states_reached) {
            states_reached->push_back(state);
        }
        assert(state_distance <= distance);
        if (state_distance < distance)
            continue;
        for (unsigned int i = 0; i < graph[state].size(); i++) {
            const auto &transition = graph[state][i];
            int successor = transition.first;
            T cost = T(transition.second);
            T successor_cost = state_distance + cost;
            if (distances[successor] > successor_cost) {
                distances[successor] = successor_cost;
                queue.push(successor_cost, successor);
            }
        }
    }
}

inline void dijkstra_search_epsilon(
    const std::vector<std::vector<std::pair<int, int>>> &graph,
    int initial_state, std::vector<int> &distances, std::vector<int> &states_reached) {
    AdaptiveQueue<int, int> queue;
    queue.push(0, initial_state);
    dijkstra_search_epsilon(graph, queue, distances, &states_reached);
}


inline void dijkstra_search_epsilon(
    const std::vector<std::vector<std::pair<int, IntEpsilon>>> &graph,
    int initial_state,
    std::vector<IntEpsilon> &distances,
    std::vector<int> &states_reached) {
    HeapQueue<IntEpsilon, int> queue;
    IntEpsilon initial_state_cost(0);
    queue.push(initial_state_cost, initial_state);
    dijkstra_search_epsilon(graph, queue, distances, &states_reached);
}
}
