#include "spanner_oracle.h"

#include "../evaluation_context.h"
#include "../evaluation_result.h"
#include "../option_parser.h"
#include "../plugin.h"
#include "planning_abstraction.h"

#include <iostream>
#include <string>
#include <unordered_map>

namespace policy_fuzzing {

SpannerQualOracle::SpannerQualOracle(const options::Options& opts)
    : Oracle(g_planning_abstraction)
    , max_heuristic::HSPMaxHeuristic(opts)
{
    std::unordered_map<std::string, int> spanner_idx;
    propositions.emplace_back();
    goal_propositions.push_back(propositions.size() - 1);
    for (auto var : task_proxy.get_variables()) {
        for (int val = 0; val < var.get_domain_size(); ++val) {
            auto fact = var.get_fact(val);
            if (fact.get_name().rfind("Atom carrying", 0) == 0) {
                const int i = fact.get_name().rfind(",") + 2;
                const int j = fact.get_name().rfind(")") - i;
                std::string spanner = fact.get_name().substr(i, j);
                auto idx = spanner_idx.emplace(
                    std::pair<std::string, int>(spanner, spanner_idx.size()));
                if (idx.second) {
                    spanners_.emplace_back(nullptr, nullptr);
                }
                spanners_[idx.first->second].first =
                    get_proposition(get_prop_id(fact));
            } else if (fact.get_name().rfind("Atom useable", 0) == 0) {
                const int i = fact.get_name().find("(") + 1;
                const int j = fact.get_name().find(")") - i;
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
}

void
SpannerQualOracle::add_options_to_parser(options::OptionParser& parser)
{
    Heuristic::add_options_to_parser(parser);
    parser.add_option<std::shared_ptr<PlanningAbstraction>>(
        "asd", "", "default_planning_abstraction");
}

tl::optional<Costs>
SpannerQualOracle::pessimist_value(StateAbstraction s, int time_out)
{
    return optimistic_value(s, time_out);
}

tl::optional<Costs>
SpannerQualOracle::optimistic_value(StateAbstraction idx, int)
{
    State s = g_planning_abstraction->get_state(idx);
    int left = 0;
    for (auto goal : task_proxy.get_goals()) {
        if (s[goal.get_pair().var] != goal) {
            ++left;
        }
    }
    if (left == 0) {
        return Costs(0);
    }
    {
        EvaluationContext ctxt(s);
        compute_result(ctxt);
    }
    int n = spanners_.size();
    for (auto p : spanners_) {
        if (p.first->cost == -1 || p.second->cost == -1) {
            --n;
        }
    }
    std::cout << left << " " << n << std::endl;
    if (n < left) {
        return policy_fuzzing::DEAD_END;
    }
    return Costs(0);
}

static Plugin<Oracle>
    _plugin("spanner_qual_oracle", options::parse<Oracle, SpannerQualOracle>);

} // namespace policy_fuzzing
