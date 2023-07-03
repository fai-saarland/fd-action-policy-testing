#include "scc.h"

#include "equivalence_relation.h"

SCC::SCC(const std::vector<std::vector<int>> &_graph) : graph(_graph) {
    compute_scc_equivalence(graph, sccs);
}

void SCC::compute_scc_graph() {
    if (!vertex_scc.empty())
        return;                     //Avoid recomputing

    int node_count = graph.size();
    //Compute the SCC associated with each vertex
    vertex_scc.resize(node_count, -1);
    for (int i = 0; i < sccs.size(); i++) {
        for (int j = 0; j < sccs[i].size(); j++) {
            vertex_scc[sccs[i][j]] = i;
        }
    }

    scc_graph.resize(sccs.size());
    //Compute the layer in the CG of each scc and var.
    scc_layer.resize(sccs.size(), -1);
    vertex_layer.resize(node_count, 0);
    //Initially all the sccs are considered to have layer 0 (root)
    for (int i = 0; i < sccs.size(); i++) {
        //If no one pointed this scc, it is a root.
        if (scc_layer[i] == -1)
            scc_layer[i] = 0;
        int layer = scc_layer[i];
        for (int j = 0; j < sccs[i].size(); j++) {
            int var = sccs[i][j];
            vertex_layer[var] = layer; //Assign the layer of the scc to its
            //variables

            //Each element pointed by var and not in scc i, it is a
            //descendant of scc i
            for (int k = 0; k < graph[var].size(); k++) {
                int var2 = graph[var][k];
                int scc_var2 = vertex_scc[var2];
                //If this is the first time we point it, we have found a
                //shortest path from the root to scc_var2
                if (scc_var2 != i) {
                    scc_graph[i].insert(scc_var2);
                    if (scc_layer[scc_var2] == -1) {
                        //cout << var << " => " << var2 << ": " << scc_var2 << " updated to " << layer +1 << endl;
                        scc_layer[scc_var2] = layer + 1;
                    }
                }
            }
        }
    }
}
