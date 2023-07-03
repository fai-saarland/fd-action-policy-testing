#include "merge_linear_criteria.h"

#include "abstraction.h"
#include "merge_criterion.h"

#include "../plugin.h"
#include <cassert>
#include <cstdlib>
#include <vector>
#include <map>

namespace simulations {
MergeLinearCriteria::MergeLinearCriteria(const options::Options &opts) :
    MergeStrategy(),
    criteria(opts.get_list<std::shared_ptr<MergeCriterion>>("criteria")),
    order(opts.get<MergeOrder>("var_order")) {
    // TODO make sure initialization order is still ok
    add_init_function([&]() {
                          int var_count = global_simulation_task()->get_num_variables();
                          if (order == MergeOrder::REVERSE_LEVEL) {
                              for (int i = 0; i < var_count; ++i)
                                  remaining_vars.push_back(i);
                          } else {
                              for (int i = var_count - 1; i >= 0; i--)
                                  remaining_vars.push_back(i);
                          }

                          if (order == MergeOrder::RANDOM) {
                              simulations_rng.shuffle(remaining_vars);
                          }

                          for (const auto &i: criteria) {
                              i->init();
                          }
                      });
}

void MergeLinearCriteria::init_strategy(const std::vector<Abstraction *> &abstractions) {
    remaining_vars.clear();
    for (auto abs: abstractions) {
        if (abs) {
            assert(abs->get_varset().size() == 1 || abs == abstractions.back());
            if (abs->get_varset().size() == 1)
                remaining_vars.push_back(*(abs->get_varset().begin()));
        }
    }

    if (order == MergeOrder::REVERSE_LEVEL) {
        std::sort(std::begin(remaining_vars), std::end(remaining_vars));
    } else if (order == MergeOrder::RANDOM) {
        simulations_rng.shuffle(remaining_vars);
    } else {
        std::sort(std::begin(remaining_vars), std::end(remaining_vars), std::greater<>());
    }


    for (const auto &i : criteria) {
        i->init();
    }
}


std::pair<int, int> MergeLinearCriteria::get_next(const std::vector<Abstraction *> &all_abstractions,
                                                  int limit_abstract_states_merge, int min_limit_abstract_states_merge,
                                                  int limit_transitions_merge) {
    assert(!done());

    int first;
    if (all_abstractions.back()->is_atomic()) {
        first = next(all_abstractions);

        std::cout << "First variable: #" << first;
        for (int i = 0; i < global_simulation_task()->get_variable_domain_size(first); ++i)
            std::cout << " " << global_simulation_task()->get_fact_name(FactPair(first, i));
        assert(all_abstractions[first]);
        std::cout << std::endl;
    } else {
        // The most recent composite abstraction is appended at the end of
        // all_abstractions in merge_and_shrink.cc
        first = all_abstractions.size() - 1;
    }
    int second;
    do {
        second = next(all_abstractions, all_abstractions[first],
                      limit_abstract_states_merge, min_limit_abstract_states_merge, limit_transitions_merge);
        if (second < 0) {
            if (remaining_vars.size() < 2) {
                return {-1, -1};
            }
            //Select another variable as first
            first = next(all_abstractions);
            std::cout << "First variable: #" << first;
            for (int i = 0; i < global_simulation_task()->get_variable_domain_size(first); ++i)
                std::cout << " " << global_simulation_task()->get_fact_name(FactPair(first, i));
            std::cout << std::endl;
        }
    } while (second < 0);

    std::cout << "Next variable: #" << second;
    for (int i = 0; i < global_simulation_task()->get_variable_domain_size(second); ++i)
        std::cout << " " << global_simulation_task()->get_fact_name(FactPair(second, i));
    std::cout << std::endl;

    assert(all_abstractions[first]);
    assert(all_abstractions[second]);
    --remaining_merges;
    return std::make_pair(first, second);
}

void MergeLinearCriteria::dump_strategy_specific_options() const {
    std::cout << "Linear merge criteria strategy: ";
    for (const auto &i : criteria) {
        std::cout << i->get_name() << "_";
    }
    switch (order) {
    case MergeOrder::LEVEL:
        std::cout << "LEVEL";
        break;
    case MergeOrder::REVERSE_LEVEL:
        std::cout << "REVERSE_LEVEL";
        break;
    case MergeOrder::RANDOM:
        std::cout << "RANDOM";
        break;
    default:
        std::cerr << "Unknown merge criterion.";
        abort();
    }
}

std::string MergeLinearCriteria::name() const {
    return "linear_criteria";
}

bool MergeLinearCriteria::is_linear() const {
    return true;
}

void MergeLinearCriteria::select_next(int var_no) {
    auto position = find(remaining_vars.begin(), remaining_vars.end(), var_no);
    assert(position != remaining_vars.end());
    remaining_vars.erase(position);
    //selected_vars.push_back(var_no);
    for (const auto &i : criteria) {
        i->select_next(var_no);
    }
}

int MergeLinearCriteria::next(const std::vector<Abstraction *> &all_abstractions,
                              Abstraction *abstraction, int limit_abstract_states_merge,
                              int min_limit_abstract_states_merge,
                              int limit_transitions_merge) {
    assert(!done());
    std::vector<int> candidate_vars(remaining_vars);

    //Remove candidate vars
    if (limit_abstract_states_merge > 0) {
        assert(abstraction);
        int limit = limit_abstract_states_merge / abstraction->size();
        int min_limit = min_limit_abstract_states_merge / abstraction->size();
        candidate_vars.erase(remove_if(std::begin(candidate_vars),
                                       std::end(candidate_vars),
                                       [&](int var) {
                                           return !all_abstractions[var] ||
                                           all_abstractions[var]->size() > limit ||
                                           (limit_transitions_merge &&
                                            abstraction->estimate_transitions(all_abstractions[var]) >
                                            limit_transitions_merge &&
                                            all_abstractions[var]->size() > min_limit);
                                       }), std::end(candidate_vars));
    }

    if (candidate_vars.empty())
        return -1;

    //Apply the criteria in order, until its finished or there is only one remaining variable
    for (int i = 0; candidate_vars.size() > 1 &&
         i < criteria.size(); ++i) {
        criteria[i]->filter(all_abstractions, candidate_vars, abstraction);
    }
    assert(!candidate_vars.empty());

    std::cout << "Candidates: ";
    for (int candidate_var : candidate_vars) {
        std::cout << candidate_var << " ";
    }
    std::cout << std::endl;

    int var = candidate_vars[0];
    select_next(var);
    return var;
}

void MergeLinearCriteria::remove_useless_vars(const std::vector<int> &useless_vars) {
    for (int v: useless_vars)
        std::cout << "Remove var from merge consideration: " << v << std::endl;
    remaining_vars.erase(std::remove_if(std::begin(remaining_vars), std::end(remaining_vars),
                                        [&](int i) {
                                            return std::find(begin(useless_vars),
                                                             end(useless_vars),
                                                             i) != std::end(useless_vars);
                                        }), std::end(remaining_vars));
}

static std::shared_ptr<MergeStrategy> _parse(options::OptionParser &parser) {
    std::vector <std::string> variable_orders{"level", "reverse_level", "random"};
    parser.add_enum_option<MergeOrder>("var_order", variable_orders,

                                       "merge variable order for tie breaking",
                                       "RANDOM");

    parser.add_list_option<std::shared_ptr<MergeCriterion>>
        ("criteria", "list of criteria for the merge linear strategy");

    Options opts = parser.parse();
    if (parser.dry_run())
        return nullptr;
    else
        return std::make_shared<MergeLinearCriteria>(opts);
}

static Plugin<MergeStrategy> _plugin("merge_linear_criteria", _parse);
}
