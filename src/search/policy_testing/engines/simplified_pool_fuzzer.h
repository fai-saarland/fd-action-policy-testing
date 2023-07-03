#pragma once

#include "../../utils/rng.h"
#include "../../utils/timer.h"
#include "../novelty_store.h"
#include "../pool.h"
#include "testing_base_engine.h"

class Evaluator;

namespace policy_testing {
class FuzzingBias;
class PoolFilter;

class SimplifiedPoolFuzzerEngine : public PolicyTestingBaseEngine {
public:
    explicit SimplifiedPoolFuzzerEngine(const options::Options &opts);
    static void add_options_to_parser(options::OptionParser &parser);

    void print_statistics() const override;

protected:
    SearchStatus step() override;

private:
    void print_status_line() const;

    /**
     * Inserts the given @param entry to the pool (if not filtered).
     * If the insertion takes place, tests the pool entry and updates the frontiers.
     * @return true iff the insertion takes place.
     */
    bool insert(PoolEntry &&entry);

    bool check_limits() const;

    // Includes all successors of pool states for which no attempt to add them to the pool has been made so far.
    // This means all states in the frontier eventually end up in the pool, or are removed because they are filtered
    std::vector<PoolEntry> frontier;

    // The list of pool states. Each given to the oracle upon insertion.
    Pool pool_;

    utils::HashSet<StateID> states_in_pool_;
    std::unique_ptr<NoveltyStore> novelty_store_;

    // set of all states that have been generated so far
    utils::HashSet<StateID> seen_;
    utils::HashMap<StateID, bool> is_dead_;

    utils::RandomNumberGenerator rng_;
    std::shared_ptr<Evaluator> eval_;
    std::shared_ptr<PoolFilter> filter_;
    std::unique_ptr<PoolFile> store_;

    const unsigned max_steps_;
    const unsigned max_pool_size_;

    utils::Timer fuzzing_time_;
    unsigned step_ = 0;
    unsigned filtered_ = 0;
    unsigned dead_ends_ = 0;
    unsigned goal_states = 0;
};
} // namespace policy_testing
