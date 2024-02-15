#pragma once

#include "../../search_algorithm.h"
#include "../../utils/hash.h"
#include "../../utils/timer.h"
#include "../bug_value.h"
#include "../testing_environment.h"
#include "../policy.h"
#include "../oracle.h"

#include <initializer_list>
#include <memory>
#include <set>

namespace policy_testing {
class Policy;
class Oracle;
struct PoolEntry;
class TestingBaseComponent;

class PolicyTestingBaseEngine : public SearchAlgorithm {
public:
    explicit PolicyTestingBaseEngine(const plugins::Options &opts);
    static void add_options_to_feature(
        plugins::Feature &feature,
        bool testing_arguments_mandatory);

    void print_statistics() const override {print_bug_statistics();}

    /**
     * Print out bug message for given state. If bugs are logged in bugs file, add entry there
     */
    void print_new_bug_info(const State &state, StateID state_id);

    static void report_initialized() {
        std::cout << "Testing engine initialized [t=" << utils::g_timer << "]" << std::endl;
    }

    /**
     * Flag a state as a bug that for instance could not be confirmed as a bug in a previous bug test,
     * for which a higher bug value has been confirmed, or which has not been handled at all.
     * Does nothing if the state is already known as a bug with the given bug_value.
     * @param state The state to flag as a bug.
     * @param test_result The corresponding test_result consisting of a bug value (must be >0) and an upper bound for hstar(state).
     * @note removes @param state from the list of non-bug states (if present).
     * @warning bug_value must be greater than 0, otherwise state is no bug.
     */
    void add_additional_bug(const State &state, TestResult test_result);

    /**
     * @brief Read the stored bug value and the associated upper cost bound (if stored) otherwise return 0.
     * Can only be used to look up upper bounds on bug states.
     * Upper bounds can be higher than the one stored internally be the oracles (especially for qualitative bug states),
     * as the respective data stored in the test engine is only updated if a better bug value has been found.
     */
    TestResult get_stored_bug_result(const State &state);

    bool is_known_bug(const State &state) {
        return get_stored_bug_result(state).bug_value > 0;
    }

    std::shared_ptr<Policy> get_policy() const {
        return policy_;
    }

    unsigned num_tests_ = 0;
    unsigned num_solved_ = 0;
    unsigned num_unsolved_state_bugs_ = 0;

protected:
    SearchStatus step() override = 0;

    void finish_initialization(std::initializer_list<TestingBaseComponent *> components);

    void set_max_time(timestamp_t max_time);
    void set_max_time(double max_time) {
        set_max_time(static_cast<timestamp_t>(std::floor(max_time)));
    }
    void run_test(const PoolEntry &entry, timestamp_t testing_time);
    void run_test(const PoolEntry &entry);

    void compute_bug_regions_print_result();
    void print_bug_statistics() const;

    TestingEnvironment env_;

    utils::HashMap<StateID, TestResult> bugs_;
    utils::HashSet<StateID> non_bugs_; // states that have been tested but that have not been reported as bugs

    std::shared_ptr<Policy> policy_;
    std::shared_ptr<Oracle> oracle_;
    std::string policy_cache_file_;
    bool write_bugs_file_;
    std::ofstream bugs_stream_;
    bool read_policy_cache_;
    bool just_write_policy_cache_;

    utils::Timer testing_timer_;

    const bool debug_;
private:
    std::set<TestingBaseComponent *> components_;

    const bool verbose_;
};
} // namespace policy_testing
