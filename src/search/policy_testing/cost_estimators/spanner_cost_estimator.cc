#include "spanner_cost_estimator.h"

#include "../../evaluation_context.h"
#include "../../option_parser.h"
#include "../../plugin.h"

#include <iostream>
#include <string>
#include <unordered_map>

namespace policy_testing {
SpannerQualPlanCostEstimator::SpannerQualPlanCostEstimator(const options::Options &opts)
    : PlanCostEstimator()
      , max_heuristic::HSPMaxHeuristic(opts) {
}

void
SpannerQualPlanCostEstimator::initialize() {
    if (initialized) {
        return;
    }
    std::unordered_map<std::string, int> spanner_idx;
    propositions.emplace_back();
    goal_propositions.push_back(propositions.size() - 1);
    for (auto var : get_task_proxy().get_variables()) {
        for (int val = 0; val < var.get_domain_size(); ++val) {
            auto fact = var.get_fact(val);
            if (fact.get_name().rfind("Atom carrying", 0) == 0) {
                const unsigned int i = fact.get_name().rfind(',') + 2;
                const unsigned int j = fact.get_name().rfind(')') - i;
                std::string spanner = fact.get_name().substr(i, j);
                auto idx = spanner_idx.emplace(
                    std::pair<std::string, int>(spanner, spanner_idx.size()));
                if (idx.second) {
                    spanners_.emplace_back(nullptr, nullptr);
                }
                spanners_[idx.first->second].first =
                    get_proposition(get_prop_id(fact));
            } else if (fact.get_name().rfind("Atom useable", 0) == 0) {
                const unsigned int i = fact.get_name().find('(') + 1;
                const unsigned int j = fact.get_name().find(')') - i;
                std::string spanner = fact.get_name().substr(i, j);
                auto idx = spanner_idx.emplace(
                    std::pair<std::string, int>(spanner, spanner_idx.size()));
                if (idx.second) {
                    spanners_.emplace_back(nullptr, nullptr);
                }
                spanners_[idx.first->second].second =
                    get_proposition(get_prop_id(fact));
            }
        }
    }
    std::cout << "Spanners: " << spanners_.size() << std::endl;
    PlanCostEstimator::initialize();
}

void
SpannerQualPlanCostEstimator::add_options_to_parser(options::OptionParser &parser) {
    Heuristic::add_options_to_parser(parser);
}

int
SpannerQualPlanCostEstimator::compute_value(const State &state) {
    int left = 0;
    for (auto goal : get_task_proxy().get_goals()) {
        if (state[goal.get_pair().var] != goal) {
            ++left;
        }
    }
    if (left == 0) {
        return 0;
    }
    {
        EvaluationContext ctxt(state);
        compute_result(ctxt);
    }
    int n = spanners_.size();
    for (auto p : spanners_) {
        if (p.first->cost == -1 || p.second->cost == -1) {
            --n;
        }
    }
    if (n < left) {
        return PlanCostEstimator::DEAD_END;
    }
    return UNKNOWN;
}

static Plugin<PlanCostEstimator>
_plugin("spanner_qual_cost_estimator", options::parse<PlanCostEstimator, SpannerQualPlanCostEstimator>);
} // namespace policy_testing
