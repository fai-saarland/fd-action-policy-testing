/**
 * @file fuzzer.h
 * @author Philippe Heim
 *
 * TODO: Document
 */
#ifndef FAST_DOWNWARD_FUZZERS_H
#define FAST_DOWNWARD_FUZZERS_H

#include <set>

#include "../basics/planning_abstraction.h"
#include "random_choices.h"

namespace policy_fuzzing {

    class FuzzingAbstraction {
    public:
        virtual bool is_bug(StateAbstraction) = 0;

        virtual double bias(StateAbstraction) = 0;

        std::vector<StateAbstraction> successors(StateAbstraction);

        std::vector<StateAbstraction> initial();

        explicit FuzzingAbstraction(PlanningAbstraction &);

        virtual ~FuzzingAbstraction() = default;

    private:
        PlanningAbstraction &planning_abstraction;

    };

    class Fuzzer {
    public:

        virtual void fuzzing_step() = 0;

        std::set<StateAbstraction> get_bugs();

        bool found_bug();

        void fuzz_steps(unsigned steps, bool stop_on_bug = false);

        void fuzz_for(double max_time, bool stop_on_bug = false);

        Fuzzer(int seed, FuzzingAbstraction &a_problem) : random(seed), problem(a_problem) {}

        virtual ~Fuzzer() = default;

    protected:
        std::set<StateAbstraction> buggy_states{};
        RandomChoices random;
        FuzzingAbstraction &problem;

    };


    class RandomWalk : public Fuzzer {
    public:
        void fuzzing_step() override;

        RandomWalk(int seed, FuzzingAbstraction &, unsigned random_walk_max_length);

        ~RandomWalk() override = default;

    private:
        unsigned const random_walk_max_length;

    };

    class MutationFuzzer : public Fuzzer {
    public:
        void fuzzing_step() override;

        MutationFuzzer(int seed, FuzzingAbstraction &, unsigned max_action_applications, bool reconsider_buggy_states);

        ~MutationFuzzer() override = default;

    private:
        unsigned const max_action_applications;
        bool const reconsider_buggy_states;

        std::vector<StateAbstraction> seed;
        unsigned seed_index = 0;

        std::vector<StateAbstraction> population_list;
        std::vector<double> population_bias;
        utils::HashSet<StateAbstraction> population_set;

    };
}

#endif
