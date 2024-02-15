#pragma once

// The new merge strategy is based on a list of criteria.  We start
// with the set of candidate variables and apply each criterion, that
// discards some variables, until only one variable is left. If more
// than one variable is left after applying all the criteria, the
// merge_order is used as final tie-breaking.
//
// CG: prefer v to v' if v has causal influence over already
// merged variables and v' not.
//
// GOAL: prefer goal variables to non-goal variables
//
// Relevant: prefer relevant variables to non-relevant variables. A
// variable is relevant if a) it is a goal or b) it has a causal
// influence over already merged variables.
//
// MinSCC: Same as CG, but applying tie-breaking in case that more
// than one variable is causally relevant: prefer variable v to v' if
// SCC(v) has a path to SCC(v') if SCC(v) has a path to SCC(v').
// Optionally, only one variable is selected per SCC
// (the one with smallest "level", i.e. closer to the SCC root)
//
// Empty: prefer variables that will make more labels of the current
// abstraction empty
//
// Num: prefer variables that appear in more labels of the current
// abstraction.

#include <vector>
#include <set>
#include <string>
#include <iostream>
#include <sstream>
#include <limits>
#include <memory>
#include "../utils/scc.h"

namespace plugins {
class Options;
class Feature;
}

namespace simulations {
class Abstraction;

class MergeCriterion {
protected:
    //Returns true in case that at least one variable fits the criterion
    bool filter(std::vector<int> &vars, const std::vector<bool> &criterion) const {
        std::vector<int> aux;
        for (int var : vars) {
            if (criterion[var]) {
                aux.push_back(var);
            }
        }
        if (!aux.empty()) {
            vars.swap(aux);
            return true;
        }
        return false;
    }

    template<class T>
    void filter_best(std::vector<int> &vars,
                     const std::vector<T> &criterion,
                     bool minimize, double opt_margin = 1, double opt_diff = 0) const {
        std::vector<int> aux;

        if (opt_diff == 0 && opt_margin == 1) {
            T best = (minimize ? std::numeric_limits<T>::max() : 0.0);
            for (int var : vars) {
                T score = criterion[var];
                if ((minimize && score < best) ||
                    (!minimize && score > best)) {
                    std::vector<int>().swap(aux);
                    best = score;
                }
                if (score == best) {
                    aux.push_back(var);
                }
            }
        } else {
            T best = (minimize ? *(std::min_element(criterion.begin(), criterion.end())) :
                      *(std::max_element(criterion.begin(), criterion.end())));
            T bestcmp = static_cast<T>(minimize ? std::max(best * opt_margin, best + opt_diff) :
                         std::min(best * opt_margin, best - opt_diff));
            std::cout << " (" << best << " " << bestcmp << ") ";
            for (int var : vars) {
                T score = criterion[var];
                if ((minimize && score <= bestcmp) ||
                    (!minimize && score >= bestcmp)) {
                    aux.push_back(var);
                }
            }
        }
        if (!aux.empty()) {
            vars.swap(aux);
        }
    }

    bool allow_incremental;
public:
    MergeCriterion() : allow_incremental(true) {}
    virtual ~MergeCriterion() = default;

    virtual void init() = 0;

    virtual void disable_incremental() {
        allow_incremental = false;
    }

    // Allows for incremental computation (currently used to compute
    // predecessor variables in the CG). However, it should only work if
    // we have not disabled this.
    virtual void select_next(int var_no) = 0;

    virtual void filter(const std::vector<Abstraction *> &all_abstractions,
                        std::vector<int> &vars, Abstraction *abstraction) = 0;

    [[nodiscard]] virtual std::string get_name() const = 0;

    virtual bool reduce_labels_before_merge() const {
        return false;
    }
};

class MergeCriterionCG : public MergeCriterion {
protected:
    std::vector<bool> is_causal_predecessor;
public:
    void init() override;

    void select_next(int var_no) override;

    void filter(const std::vector<Abstraction *> &all_abstractions,
                std::vector<int> &vars, Abstraction *abstraction) override;

    [[nodiscard]] std::string get_name() const override {
        return "CG";
    }
};

class MergeCriterionGoal : public MergeCriterion {
    std::vector<bool> is_goal_variable;
public:
    void init() override;

    void select_next(int var_no) override;

    void filter(const std::vector<Abstraction *> &all_abstractions,
                std::vector<int> &vars, Abstraction *abstraction) override;

    [[nodiscard]] std::string get_name() const override {
        return "GOAL";
    }
};

class MergeCriterionRelevant : public MergeCriterionCG {
    void init() override;

    [[nodiscard]] std::string get_name() const override {
        return "RELEVANT";
    }
};

class MergeCriterionMinSCC : public MergeCriterion {
    const bool reverse;
    const bool tie_by_level;
    // whether to use pre_to_eff or complete cg
    const bool complete_cg;

    std::vector<bool> is_causal_predecessor;
    std::unique_ptr<SCC> scc;

    void forbid_scc_descendants(int scc_index,
                                const std::vector<std::set<int>> &scc_graph,
                                std::vector<bool> &forbidden_sccs) const;

public:
    explicit MergeCriterionMinSCC(bool reverse_ = false,
                                  bool tie_by_level_ = true,
                                  bool complete_cg_ = false) :
        reverse(reverse_),
        tie_by_level(tie_by_level_),
        complete_cg(complete_cg_),
        scc(nullptr) {
    }

    explicit MergeCriterionMinSCC(const plugins::Options &opts);

    ~MergeCriterionMinSCC() override = default;

    void init() override;

    void select_next(int var_no) override;

    void filter(const std::vector<Abstraction *> &all_abstractions,
                std::vector<int> &vars, Abstraction *abstraction) override;

    [[nodiscard]] std::string get_name() const override {
        return "SCC";
    }
};

class MergeCriterionTRs : public MergeCriterion {
    const bool only_goals;
    const bool only_empty;

    // By default, this criterion discards all the variables that do
    // have the maximum number of shared TRs.  In practice, however,
    // if the maximum is 1000 and there is a variable with 9999, it
    // may make sense to let that as well.  We add two parameters to
    // control the amount of suboptimality allowed (in terms of a
    // factor or a difference)
    // allowed_value = min(best*opt_factor, best - opt_diff)
    const double opt_factor;
    const int opt_diff;
public:
    explicit MergeCriterionTRs(const plugins::Options &opts);

    void init() override;

    void select_next(int var_no) override;

    void filter(const std::vector<Abstraction *> &all_abstractions,
                std::vector<int> &vars, Abstraction *abstraction) override;

    [[nodiscard]] std::string get_name() const override {
        std::stringstream ss;
        ss << "TRs(";
        if (only_goals)
            ss << "goals ";
        if (only_empty)
            ss << "empty";
        ss << ")";
        return ss.str();
    }

    [[nodiscard]] bool reduce_labels_before_merge() const override {
        return true;
    }
};
}
