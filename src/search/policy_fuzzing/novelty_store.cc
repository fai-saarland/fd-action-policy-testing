#include "novelty_store.h"

#include "../task_proxy.h"

#include <iostream>
#include <cassert>

namespace policy_fuzzing {

struct VarsetIterator {
    explicit VarsetIterator(unsigned num_vars, unsigned varset_size)
        : vars_(varset_size)
        , num_vars_(num_vars)
    {
        for (unsigned i = 0; i < varset_size; ++i) {
            vars_[i] = i;
        }
        idx_ = 0;
    }

    const std::vector<unsigned>& operator*() const { return vars_; }
    bool next()
    {
        int i = vars_.size() - 1;
        while (i >= 0 && ++vars_[i] == (num_vars_ - (vars_.size() - i + 1))) {
            --i;
        }
        if (i < 0) {
            return false;
        }
        ++i;
        for (; i < static_cast<int>(vars_.size()); ++i) {
            vars_[i] = vars_[i - 1] + 1;
        }
        ++idx_;
        return true;
    }
    unsigned get_idx() const { return idx_; }

    std::vector<unsigned> vars_;
    const unsigned num_vars_;
    unsigned idx_;
};

NoveltyStore::NoveltyStore(unsigned max_arity, const std::vector<int>& domains)
    : max_arity_(std::min(static_cast<unsigned>(domains.size()), max_arity))
    , domains_(domains.size())
    , offsets_(max_arity_)
    , fact_sets_(max_arity_)
{
    if (max_arity_ > 0) {
        for (int i = domains.size() - 1; i >= 0; --i) {
            domains_[i] = domains[i];
        }
        for (unsigned i = 0; i < max_arity_; ++i) {
            FactSetType offset = 0;
            VarsetIterator varsets(domains.size(), i + 1);
            offsets_[i].push_back(0);
            do {
                const auto& vars = *varsets;
                FactSetType product = 1;
                for (int j = i; j >= 0; --j) {
                    product *= domains_[vars[j]];
                }
                offset += product;
                offsets_[i].push_back(offset);
            } while (varsets.next());
        }
    }
}

int
NoveltyStore::compute_novelty(const State& state)
{
    for (unsigned i = 0; i < max_arity_; ++i) {
        VarsetIterator varsets(domains_.size(), i + 1);
        do {
            const auto& vars = *varsets;
            FactSetType res = offsets_[i][varsets.get_idx()];
            FactSetType product = 1;
            for (unsigned j = 0; j <= i; ++j) {
                res += product * state[vars[j]].get_value();
                product *= domains_[vars[j]];
            }
            if (!fact_sets_[i].count(res)) {
                return i;
            }
        } while (varsets.next());
    }
    return -1;
}

bool
NoveltyStore::insert(const State& state)
{
    bool is_novel = false;
    for (unsigned i = 0; i < max_arity_; ++i) {
        VarsetIterator varsets(domains_.size(), i + 1);
        do {
            const auto& vars = *varsets;
            FactSetType res = offsets_[i][varsets.get_idx()];
            FactSetType product = 1;
            for (unsigned j = 0; j <= i; ++j) {
                res += product * state[vars[j]].get_value();
                product *= domains_[vars[j]];
            }
            if (fact_sets_[i].insert(res).second) {
                is_novel = true;
            }
        } while (varsets.next());
    }
    return is_novel;
}

} // namespace policy_fuzzing
