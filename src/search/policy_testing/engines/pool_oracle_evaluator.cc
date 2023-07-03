#include "pool_oracle_evaluator.h"

#include "../../evaluation_context.h"
#include "../../evaluator.h"
#include "../../option_parser.h"
#include "../../plugin.h"
#include "../../utils/countdown_timer.h"
#include "../cost_estimator.h"
#include "../out_of_resource_exception.h"

#include <iomanip>
#include <iostream>

// pool oracle evaluator might be broken after having changed InternalPlannerPlanCostEstimator::compute_value

namespace policy_testing {
PoolOracleEvaluatorEngine::PoolOracleEvaluatorEngine(
    const options::Options &opts)
    : PolicyTestingBaseEngine(opts)
      , env_(task, &state_registry)
      , pool_(load_pool_file(
                  task,
                  state_registry,
                  opts.get<std::string>("pool_file")))
      , cost_estimator_(opts.get<std::shared_ptr<PlanCostEstimator>>("oracle"))
      , eval_(
          opts.contains("eval") ? opts.get<std::shared_ptr<Evaluator>>("eval")
                                : nullptr)
      , step_time_limit_(opts.get<int>("step_max_time"))
      , max_steps_(opts.get<int>("max_steps"))
      , first_step_(opts.get<int>("start_from"))
      , end_step_(
          first_step_ + max_steps_ > pool_.size() ? pool_.size()
                                                  : first_step_ + max_steps_)
      , debug_(opts.get<bool>("debug"))
      , step_(first_step_) {
    cost_estimator_->connect_environment(&env_);
}

void
PoolOracleEvaluatorEngine::add_options_to_parser(options::OptionParser &parser) {
    parser.add_option<std::shared_ptr<PlanCostEstimator>>("oracle");
    parser.add_option<std::string>("pool_file");
    parser.add_option<std::shared_ptr<Evaluator>>("eval", "", options::OptionParser::NONE);
    parser.add_option<int>("step_max_time", "", "infinity");
    parser.add_option<int>("start_from", "", "0");
    parser.add_option<int>("max_steps", "", "infinity");
    parser.add_option<bool>("debug", "", "false");
    SearchEngine::add_options_to_parser(parser);
}

void
PoolOracleEvaluatorEngine::print_statistics() const {
    std::cout << "Solvable states: " << solved_ << std::endl;
    std::cout << "Unsolvable states: " << dead_ends_ << std::endl;
    std::cout << "Unknown states: " << unknown_ << std::endl;
}

SearchStatus
PoolOracleEvaluatorEngine::step() {
    if (step_ >= end_step_) {
        // finish_testing();
        return FAILED;
    }

    const PoolEntry &entry = pool_[step_];
    ++step_;

    std::cout << "Entry " << std::setw(5) << step_ << " / " << pool_.size()
              << " [t=" << utils::g_timer << "] ..." << std::endl;

    if (debug_) {
        const State s = entry.state;
        std::cout << "(Debug) state " << s.get_id() << ":\n";
        for (auto fact : s) {
            std::cout << "(Debug)  " << fact.get_name() << "\n";
        }
        std::cout << std::flush;
    }

    int oracle_value = PlanCostEstimator::UNKNOWN;
    if (eval_ != nullptr) {
        EvaluationContext ctxt(entry.state);
        EvaluationResult res = eval_->compute_result(ctxt);
        if (res.is_infinite()) {
            oracle_value = PlanCostEstimator::DEAD_END;
        }
    }

    if (oracle_value == PlanCostEstimator::UNKNOWN) {
        int remaining = timer->get_remaining_time();
        if (remaining < 0 || remaining > step_time_limit_) {
            remaining = step_time_limit_;
        }

        try {
            utils::reserve_extra_memory_padding(75);
            cost_estimator_->set_max_time(remaining);
            oracle_value = cost_estimator_->compute_value(entry.state);
            utils::release_extra_memory_padding();
        } catch (const OutOfResourceException &) {
            utils::release_extra_memory_padding();
            std::cout.clear();
            std::cerr.clear();
            std::cout << "time limit reached";
            std::cout << " [t=" << utils::g_timer << "]" << std::endl;
            return IN_PROGRESS;
        }
    }

    std::cout << "Oracle result for " << entry.state.get_id() << " [" << step_
              << "]: ";
    if (oracle_value != PlanCostEstimator::UNKNOWN) {
        if (oracle_value == PlanCostEstimator::DEAD_END) {
            std::cout << "dead end";
            ++dead_ends_;
        } else {
            std::cout << "plan_cost=" << oracle_value;
            ++solved_;
        }
    } else {
        std::cout << "unknown";
        ++unknown_;
    }
    std::cout << " [t=" << utils::g_timer << "]" << std::endl;

    return IN_PROGRESS;
}

static Plugin<SearchEngine> _plugin(
    "pool_oracle_evaluator",
    options::parse<SearchEngine, PoolOracleEvaluatorEngine>);
} // namespace policy_testing
