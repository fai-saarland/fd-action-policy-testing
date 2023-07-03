#pragma once

#include "../../search_engine.h"
#include "../../utils/hash.h"
#include "../../utils/timer.h"
#include "../bug_store.h"
#include "../testing_environment.h"

#include <initializer_list>
#include <memory>
#include <set>

namespace policy_testing {
class Policy;
class Oracle;
struct PoolEntry;
class TestingBaseComponent;

class PolicyTestingBaseEngine : public SearchEngine {
public:
    explicit PolicyTestingBaseEngine(const options::Options &opts);
    static void add_options_to_parser(
        options::OptionParser &parser,
        bool testing_arguments_mandatory);

    void print_statistics() const override {print_bug_statistics();}
    void print_timed_statistics() const override { }

    static void print_new_bug_info(const State &state, StateID state_id);
    static void report_initialized() {
        std::cout << "Testing engine initialized [t=" << utils::g_timer << "]" << std::endl;
    }

    /**
     * Flag a state as a bug that for instance could not be confirmed as a bug in a previous bug test,
     * for which a higher bug value has been confirmed, or which has not been handled at all.
     * Does nothing if the state is already known as a bug with the given bug_value.
     * @param state The state to flag as a bug.
     * @param bug_value The corresponding bug value (must be >0).
     * @note removes @param state from the list of non-bug states (if present).
     * @warning bug_value must be greater than 0, otherwise state is no bug.
     */
    void add_additional_bug(const State &state, BugValue bug_value);

    /**
     * @brief Read the stored bug value (if stored) otherwise return 0.
     */
    BugValue get_stored_bug_value(const State &state);

    bool is_known_bug(const State &state) {
        return get_stored_bug_value(state) > 0;
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

    void set_max_time(int max_time);
    void run_test(const PoolEntry &entry, int testing_time);
    void run_test(const PoolEntry &entry);

    void compute_bug_regions_print_result();
    void print_bug_statistics() const;

    TestingEnvironment env_;

    utils::HashMap<StateID, BugValue> bugs_;
    utils::HashSet<StateID> non_bugs_;

    std::shared_ptr<Policy> policy_;
    std::shared_ptr<Oracle> oracle_;
    std::unique_ptr<BugStoreFile> bugs_store_;
    std::string policy_cache_file_;
    bool read_policy_cache_;
    bool just_write_policy_cache_;

    utils::Timer testing_timer_;

    const bool debug_;
private:
    std::set<TestingBaseComponent *> components_;

    const bool verbose_;
};
} // namespace policy_testing
