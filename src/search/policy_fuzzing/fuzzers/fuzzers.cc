#include "fuzzers.h"

#include "../../utils/countdown_timer.h"

namespace policy_fuzzing {

    std::vector<StateAbstraction> FuzzingAbstraction::successors(StateAbstraction state) {
        return planning_abstraction.get_successors(state);
    }

    std::vector<StateAbstraction> FuzzingAbstraction::initial() {
        return planning_abstraction.get_initial_states();
    }

    FuzzingAbstraction::FuzzingAbstraction(PlanningAbstraction &a_planning_abstraction) :
            planning_abstraction(a_planning_abstraction) {}

    void Fuzzer::fuzz_steps(unsigned int steps, bool stop_on_bug) {
        for (unsigned i = 0; i < steps; i++) {
            if (stop_on_bug && found_bug())
                return;
            fuzzing_step();
        }
    }

    void Fuzzer::fuzz_for(double max_time, bool stop_on_bug) {
        utils::CountdownTimer timer(max_time);
        while (!timer.is_expired()) {
            if (stop_on_bug && found_bug())
                return;
            fuzzing_step();
        }
    }

    bool Fuzzer::found_bug() {
        return !buggy_states.empty();
    }

    std::set<StateAbstraction> Fuzzer::get_bugs() {
        return buggy_states;
    }

    void RandomWalk::fuzzing_step() {
        // Initialize the successors with the initial states (i.e. we have a virtual single initial state)
        std::vector<StateAbstraction> successors = problem.initial();

        for (unsigned i = 0; i <= random_walk_max_length && !successors.empty(); i++) {
            // Compute bias for the successors and use this to choose the current state randomly
            std::vector<double> bias;
            bias.reserve(successors.size());
            for (auto successor: successors)
                bias.push_back(problem.bias(successor));

            bool zeroes_only = true;
            for (auto bias_value: bias) {
                zeroes_only = zeroes_only && bias_value <= 0.0;
            }

            StateAbstraction state;
            if (zeroes_only)
                state = successors[random.pick_uniformly(successors.size())];
            else
                state = successors[random.dynamic_discrete_distribution(bias)];

            // Check if current state is a bug
            if (buggy_states.count(state) == 0) {
                if (problem.is_bug(state)) {
                    buggy_states.insert(state);
                    return;
                }
            }

            // set next successors
            successors = problem.successors(state);
        }

    }

    RandomWalk::RandomWalk(int seed, FuzzingAbstraction &a_problem, unsigned a_random_walk_max_length) :
            Fuzzer(seed, a_problem),
            random_walk_max_length(a_random_walk_max_length) {}

    void MutationFuzzer::fuzzing_step() {
        StateAbstraction state = 0;

        if (seed_index < seed.size()) {
            state = seed[seed_index];
            seed_index++;
        } else if (!population_list.empty()) {
            state = population_list[random.dynamic_discrete_distribution(population_bias)];

            unsigned applied_actions = random.pick_uniformly(max_action_applications) + 1;
            for (unsigned i = 0; i < applied_actions; i++) {
                std::vector<StateAbstraction> successors = problem.successors(state);
                size_t successor_index = random.pick_uniformly(successors.size());
                state = successors[successor_index];
            }
        } else {
            return;
        }

        if ((population_set.count(state) > 0) || (buggy_states.count(state) > 0))
            return;


        bool const is_bug = problem.is_bug(state);
        if (is_bug)
            buggy_states.insert(state);

        if (!is_bug || reconsider_buggy_states) {
            population_set.insert(state);
            population_list.push_back(state);
            population_bias.push_back(problem.bias(state));
        }
    }

    MutationFuzzer::MutationFuzzer(int a_seed, FuzzingAbstraction &a_problem, unsigned a_max_action_applications,
                                   bool a_reconsider_buggy_states) :
            Fuzzer(a_seed, a_problem),
            max_action_applications(a_max_action_applications),
            reconsider_buggy_states(a_reconsider_buggy_states),
            seed(problem.initial()) {}

}