#pragma once

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class State;
class AbstractTask;

namespace policy_testing {
class NoveltyStore {
public:
    explicit NoveltyStore(
        unsigned max_arity,
        const std::shared_ptr<AbstractTask> &task);
    ~NoveltyStore() = default;

    int compute_novelty(const State &state);
    bool insert(const State &state);
    [[nodiscard]] bool has_unique_factset(const State &state, unsigned arity) const;

    [[nodiscard]] unsigned size(unsigned arity) const;
    [[nodiscard]] unsigned get_arity() const;

    void print_statistics() const;

private:
    using FactSetType = unsigned long long;
    const unsigned max_arity_;
    std::vector<unsigned> domains_;
    std::vector<std::vector<FactSetType>> offsets_;
    std::vector<std::unordered_map<FactSetType, int>> fact_sets_;
};
} // namespace policy_testing
