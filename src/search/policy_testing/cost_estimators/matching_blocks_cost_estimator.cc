#include "matching_blocks_cost_estimator.h"

#include "../../option_parser.h"
#include "../../plugin.h"

#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace policy_testing {
MatchingBlocksQualPlanCostEstimator::MatchingBlocksQualPlanCostEstimator(const options::Options &)
    : PlanCostEstimator() {
}

void
MatchingBlocksQualPlanCostEstimator::initialize() {
    if (initialized) {
        return;
    }
    std::unordered_map<std::string, std::pair<int, bool>> block_ids;
    std::unordered_set<std::string> pos_hands;
    std::unordered_set<std::string> neg_hands;

    for (auto op : get_task_proxy().get_operators()) {
        bool pos = false;
        bool match = false;
        if (op.get_name().rfind("putdown-pos-pos", 0) == 0) {
            pos = true;
            match = true;
        } else if (op.get_name().rfind("putdown-neg-neg", 0) == 0) {
            match = true;
        }
        if (match) {
            const unsigned int i = 15;
            const unsigned int j = op.get_name().find(' ', i + 1);
            std::string hand = op.get_name().substr(i + 1, j - i - 1);
            std::string block = op.get_name().substr(j + 1);
            block_ids.insert(std::make_pair(
                                 block, std::pair<int, bool>(block_ids.size(), pos)));
            if (pos) {
                pos_hands.insert(hand);
            } else {
                neg_hands.insert(hand);
            }
        }
    }

    block_solid_.resize(block_ids.size(), std::pair<int, int>(-1, -1));
    wrong_hand_.resize(block_ids.size(), std::pair<int, int>(-1, -1));

    for (auto var : get_task_proxy().get_variables()) {
        for (int val = 0; val < var.get_domain_size(); ++val) {
            auto fact = var.get_fact(val);
            if (fact.get_name().rfind("Atom solid", 0) == 0) {
                auto it = block_ids.find(
                    fact.get_name().substr(11, fact.get_name().size() - 12));
                block_solid_[it->second.first] =
                    std::pair<int, int>(var.get_id(), val);
            } else if (fact.get_name().rfind("Atom holding", 0) == 0) {
                const unsigned int i = 13;
                const unsigned int j = fact.get_name().find(',', i + 1);
                const unsigned int k = fact.get_name().find(')');
                std::string hand = fact.get_name().substr(i, j - i);
                std::string block = fact.get_name().substr(j + 2, k - j - 2);
                auto it = block_ids.find(block);
                if (it->second.second
                    != static_cast<bool>(pos_hands.count(hand))) {
                    wrong_hand_[it->second.first] =
                        std::pair<int, int>(var.get_id(), val);
                }
            }
        }
    }

    for (auto g : get_task_proxy().get_goals()) {
        const unsigned int i = std::string("Atom on(").size();
        const unsigned int j = g.get_name().find(',', i + 1);
        const unsigned int k = g.get_name().find(')', j + 1);
        std::string b1 = g.get_name().substr(j + 2, k - j - 2);
        auto it = block_ids.find(b1);
        must_remain_solid_.push_back(it->second.first);
    }

    {
        std::cout << "pos-hands: ";
        bool sep = false;
        for (const auto &hand : pos_hands) {
            std::cout << (sep ? ", " : "") << hand;
            sep = true;
        }
        std::cout << std::endl;
    }

    {
        std::cout << "neg-hands: ";
        bool sep = false;
        for (const auto &hand : neg_hands) {
            std::cout << (sep ? ", " : "") << hand;
            sep = true;
        }
        std::cout << std::endl;
    }

    {
        std::cout << "pos-blocks: ";
        bool sep = false;
        for (auto &block_id : block_ids) {
            if (block_id.second.second) {
                std::cout << (sep ? ", " : "") << block_id.first << " (" << block_id.second.first << ")";
                sep = true;
            }
        }
        std::cout << std::endl;
    }

    {
        std::cout << "neg-blocks: ";
        bool sep = false;
        for (auto &block_id : block_ids) {
            if (!block_id.second.second) {
                std::cout << (sep ? ", " : "") << block_id.first << " (" << block_id.second.first << ")";
                sep = true;
            }
        }
        std::cout << std::endl;
    }
    PlanCostEstimator::initialize();
}

void
MatchingBlocksQualPlanCostEstimator::add_options_to_parser(options::OptionParser &) {
}

int
MatchingBlocksQualPlanCostEstimator::compute_value(const State &state) {
    for (const auto block_id : must_remain_solid_) {
        const auto &solid_fact = block_solid_[block_id];
        const auto &wrong_hand_fact = wrong_hand_[block_id];
        if (state[solid_fact.first].get_value() != solid_fact.second
            || state[wrong_hand_fact.first].get_value()
            == wrong_hand_fact.second) {
            return DEAD_END;
        }
    }
    return UNKNOWN;
}

static Plugin<PlanCostEstimator> _plugin(
    "matching_blocks_qual_plan_cost_estimator",
    options::parse<PlanCostEstimator, MatchingBlocksQualPlanCostEstimator>);
} // namespace policy_testing
