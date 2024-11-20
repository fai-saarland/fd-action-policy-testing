#pragma once

#include "../../utils/rng.h"
#include "../../utils/timer.h"
#include "../pool.h"
#include "../utils/novelty_store.h"
#include "testing_base_engine.h"

class Evaluator;

namespace policy_testing {
class FuzzingBias;
class PoolFilter;

class PoolFuzzerEngine : public PolicyTestingBaseEngine {
public:
    explicit PoolFuzzerEngine(const plugins::Options &opts);
    static void add_options_to_feature(plugins::Feature &feature);
    void print_statistics() const override;

protected:
    SearchStatus step() override;

private:

    /**
     * Print status information.
     */
    void print_status_line() const;

    /**
     * Conduct a guided random walk.
     * If this results in a new state, call insert to add it to the pool.
     */
    void random_walk();

    /**
     * Inserts state to pool and triggers test run on state.
     * @param ref index of reference state (parent state) in pool
     * @param steps number of steps conducted in random walk
     * @param state
     * @return true iff can be inserted (not filtered out)
     */
    bool insert(int ref, int steps, const State &state);

    /**
     * Check size and resource limits.
     * @return true if pool size is reached or if out of time or memory.
     */
    bool limits_reached() const;

    Pool pool;
    utils::HashSet<StateID> states_in_pool;
    NoveltyStore novelty_store;

    // marks states that are not worthy to be further considered by the fuzzer
    // (not necessarily dead ends)
    utils::HashMap<StateID, bool> is_dead;
    utils::RandomNumberGenerator rng;
    std::shared_ptr<Evaluator> eval;
    std::shared_ptr<FuzzingBias> bias;
    std::shared_ptr<PoolFilter> filter;

    std::unique_ptr<PoolFile> store;

    const unsigned max_steps;
    const unsigned max_pool_size;
    const int max_walk_length;
    const bool penalize_policy_fails;
    const unsigned int bias_budget;
    const bool cache_bias;

    utils::Timer fuzzing_time;
    unsigned fuzzing_step = 0;
    unsigned duplicates = 0;
    unsigned failed = 0;
    unsigned filtered = 0;
    unsigned intermediate_states = 0;

    utils::HashMap<StateID, bool> bias_cache;
};
} // namespace policy_testing
