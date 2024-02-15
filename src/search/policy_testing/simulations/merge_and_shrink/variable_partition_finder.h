#pragma once

#include <vector>
#include <set>
#include <map>

namespace simulations {
class VariablePartitionFinder {
protected:
    std::vector<std::vector<int>> partitions;      //Variables contained in each cluster
    const int limit_size;

    virtual void find() = 0;

public:
    explicit VariablePartitionFinder(int limit) : limit_size(limit) {}
    virtual ~VariablePartitionFinder() = default;

    void dump() const;

    const std::vector<std::vector<int>> &get_partition() {
        if (partitions.empty()) {
            find();
        }
        return partitions;
    }
};

class VariablePartitionGreedy : public VariablePartitionFinder {
    std::vector<int> part_size;     // size of each cluster
    std::map<int, std::map<int, std::set<int>>> weights;     //We store the index of the operators

    void init();

    void merge(int p1, int p2);

    //Selects the pair with maximum weight so that size is below the threshold
    [[nodiscard]] std::pair<int, int> pick_parts() const;

protected:
    void find() override;

public:
    explicit VariablePartitionGreedy(int limit) : VariablePartitionFinder(limit) {}
    ~VariablePartitionGreedy() override = default;
};
}
