#ifndef POLICIES_HEURISTIC_POLICY_H
#define POLICIES_HEURISTIC_POLICY_H

#include "../policy.h"
#include "../heuristic.h"

#include <vector>
#include <memory>

namespace successor_generator {
    class SuccessorGenerator;
}
class FactProxy;
class OperatorProxy;
class PolicyResult;

namespace policies {

/*
    Policy which uses an heuristic and only follows it by choosing
    the action leading to the most promising state (according to the heuristic)
*/
class HeuristicPolicy : public Policy {
    std::shared_ptr<Evaluator> evaluator;
    StateRegistry registry;
protected:
    const successor_generator::SuccessorGenerator &successor_generator;

    virtual PolicyResult compute_policy(const State &state) override;
public:
    HeuristicPolicy(const options::Options &options);
    ~HeuristicPolicy();
    virtual bool dead_ends_are_reliable() const override;
};


class LookaheadHeuristicPolicy: public Policy {
    std::shared_ptr<Evaluator> evaluator;
    StateRegistry registry;
    int const lookahead;
protected:
    const successor_generator::SuccessorGenerator &successor_generator;

    int get_max_evaluator_value(const State &state, int depth);

    PolicyResult compute_policy(const State &state) override;
public:
    explicit LookaheadHeuristicPolicy(const options::Options &options);
    ~LookaheadHeuristicPolicy() override;
    bool dead_ends_are_reliable() const override;
};

}

#endif
