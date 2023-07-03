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

class PoolFuzzerEngine : public PolicyTestingBaseEngine {
public:
    explicit PoolFuzzerEngine(const options::Options &opts);
    static void add_options_to_parser(options::OptionParser &parser);

    void print_statistics() const override;

protected:
    SearchStatus step() override;

private:
    void print_status_line() const;
    void random_walk();
    bool insert(int ref, int steps, const State &state);
    bool check_limits() const;

    Pool pool;
    utils::HashSet<StateID> states_in_pool;
    NoveltyStore novelty_store;
    utils::HashMap<StateID, bool> is_dead;  // marks states that are not worthy to be further considered by the parser (not necessarily dead ends)

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
