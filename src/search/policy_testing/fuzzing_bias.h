#pragma once

#include "component.h"
#include "../utils/rng.h"
#include "../task_proxy.h"
#include "policy.h"

namespace policy_testing {
class FuzzingBias : public TestingBaseComponent {
public:

    // the highest possible bias
    static constexpr const int POSITIVE_INFINITY = std::numeric_limits<int>::max();

    // the lowest possible bias, states that are associated with this bias need to be ignored
    static constexpr const int NEGATIVE_INFINITY = std::numeric_limits<int>::min();

    /**
     * Select a state from a vector of states with respect to a vector of weights.
     * @param rng The random number generator to use.
     * @param vec Vector of states.
     * @param weights Set of weights. If a state has weight POSITIVE_INFINITY, then only states with weight
     * POSITIVE_INFINITY will be considered. States with weight NEGATIVE_INFINITY are ignored.
     * @warning finite negative weights are not supported
     * @return a pointer to the selected state if a state has been selected or nullptr otherwise.
     */
    static const State *weighted_choose(utils::RandomNumberGenerator &rng, const std::vector<State> &vec,
                                        const std::vector<int> &weights) {
        assert(vec.size() == weights.size());
        if (vec.empty()) {
            return nullptr;
        }
        // split weights with respect to whether they are finite or not
        std::vector<size_t> pos_infinite_weight_indices;
        std::vector<size_t> finite_weight_indices;
        std::vector<int> finite_weights;
        for (size_t i = 0; i < vec.size(); ++i) {
            const int weight = weights[i];
            if (weight == POSITIVE_INFINITY) {
                pos_infinite_weight_indices.emplace_back(i);
            } else if (weight == NEGATIVE_INFINITY) {
                continue;
            } else if (weight >= 0) {
                finite_weight_indices.emplace_back(i);
                finite_weights.emplace_back(weight);
            } else {
                throw std::logic_error("Finite negative weights are not supported\n");
            }
        }

        if (!pos_infinite_weight_indices.empty()) {
            // in case there are positively infinite weights, only consider these positions
            return &vec[*rng.choose(pos_infinite_weight_indices)];
        } else if (!finite_weight_indices.empty()) {
            // select index with respect to the given finite weights
            const int sum = std::accumulate(finite_weights.begin(), finite_weights.end(), 0);
            // select a random position if every entry is 0
            if (sum == 0) {
                return &vec[*rng.choose(finite_weight_indices)];
            }
            // otherwise select index based on weights
            double sample = rng() * sum;
            size_t index = 0;
            for (size_t i = 0; i < weights.size(); ++i) {
                sample -= weights[i];
                if (sample < 0) {
                    index = i;
                    break;
                }
            }
            return &vec[index];
        } else {
            // no state has a weight other than NEGATIVE_INFINITY
            return nullptr;
        }
    }


    virtual void print_statistics() const { }

    /**
     * Return weight. The bias can either be POSITIVE_INFINITY (then no state with lower bias should be selected),
     * NEGATIVE_INFINITY (then this states should never be selected, even if it is the only candidate), or a
     * finite non-negative value.
     **/
    virtual int bias(const State &, unsigned int /*budget*/) = 0;

    /**
     * Check if the bias can determine that the given state s should not be considered, e.g., because a safe heuristic s
     * associated with the bias returns \infty for s.
     */
    virtual bool can_exclude_state(const State &) = 0;

    /**
     * Check if the policy is known to fail on s executing only the amount of steps needed for the bias computation.
     * Returns false if the bias computation does not include running the policy.
     */
    virtual bool policy_is_known_to_fail(const State &, unsigned int /*budget*/) {return false;}

    /**
     * Notify if a new state was inserted into the pool.
     */
    virtual void notify_inserted(const State &) { }

    /**
     * Determine the budget used up by computing the bias.
     */
    virtual unsigned int determine_used_budget(const State &, unsigned int /* max_budget */) {
        return 0;
    }

protected:
    /**
     * TestingBaseComponent initialization. Overwrite this method to initialize
     * the object once a connection to the environment has been established.
     * Initialization is called prior to the first bias and notify_inserted
     * calls.
     **/
    void initialize() override { }
};

// make NeutralBias an extra class in order to make bias and can_exclude_state pure virtual
// so that they need to be implemented in every bias definition
class NeutralBias : public FuzzingBias {
public:
    int bias(const State &, unsigned int) override {return 1;}
    bool can_exclude_state(const State &) override {return false;}
};


class PolicyBasedBias : public FuzzingBias {
public:
    explicit PolicyBasedBias(const options::Options &opts);
    static void add_options_to_parser(options::OptionParser &parser);

    /// determine whether policy will fail on s, execution limited by horizon
    /// if result is unclear, return false
    bool policy_is_known_to_fail(const State &s, unsigned int budget) override;

    /// determine the budget used up by the policy
    unsigned int determine_used_budget(const State &s, unsigned int budget) override {
        unsigned int step_limit = get_step_limit(budget);
        unsigned int path_length = policy->execute_get_path_fragment(s, step_limit, false).size();
        assert(path_length);
        return path_length - 1;
    }

protected:
    std::shared_ptr<Policy> policy;
    const unsigned int horizon;

    unsigned int get_step_limit(unsigned int budget) {
        unsigned int step_limit = 0;
        if (budget && horizon) {
            step_limit = std::min<unsigned int>(budget, horizon);
        } else if (horizon) {
            step_limit = horizon;
        } else if (budget) {
            step_limit = budget;
        }
        return step_limit;
    }
};
} // namespace policy_testing
