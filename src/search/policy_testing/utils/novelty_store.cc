#include "novelty_store.h"

#include "../../abstract_task.h"
#include "../../task_proxy.h"

#include <cassert>
#include <iostream>

#define DMSG(x)

namespace policy_testing {
struct VarsetIterator {
    explicit VarsetIterator(unsigned num_vars, unsigned varset_size)
        : vars_(varset_size), num_vars_(num_vars) {
        for (unsigned i = 0; i < varset_size; ++i) {
            vars_[i] = i;
        }
        idx_ = 0;
    }

    const std::vector<unsigned> &operator*() const {return vars_;}

    bool next() {
        int i = static_cast<int>(vars_.size()) - 1;
        while (i >= 0 && ++vars_[i] == (num_vars_ - (vars_.size() - i - 1))) {
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

    [[nodiscard]] unsigned get_idx() const {return idx_;}

    std::vector<unsigned> vars_;
    const unsigned num_vars_;
    unsigned idx_;
};

NoveltyStore::NoveltyStore(unsigned max_arity, const std::shared_ptr<AbstractTask> &task)
    : max_arity(std::min(static_cast<unsigned>(task->get_num_variables()), max_arity)),
      domains(task->get_num_variables()),
      offsets(max_arity),
      fact_sets(max_arity) {
    if (max_arity > 0) {
        for (int i = static_cast<int>(domains.size()) - 1; i >= 0; --i) {
            domains[i] = task->get_variable_domain_size(i);
        }
        for (unsigned i = 0; i < max_arity; ++i) {
            FactSetType offset = 0;
            VarsetIterator varsets(domains.size(), i + 1);
            offsets[i].push_back(0);
            do {
                const auto &vars = *varsets;
                FactSetType product = 1;
                for (int j = static_cast<int>(i); j >= 0; --j) {
                    // TODO assertion can fail
                    assert(vars[j] < domains.size());
                    product *= domains[vars[j]];
                }
                offset += product;
                offsets[i].push_back(offset);
            } while (varsets.next());
        }
    }
}

int
NoveltyStore::compute_novelty(const State &state) {
    for (unsigned i = 0; i < max_arity; ++i) {
        VarsetIterator varsets(domains.size(), i + 1);
        do {
            const auto &vars = *varsets;
            FactSetType res = offsets[i][varsets.get_idx()];
            FactSetType product = 1;
            for (unsigned j = 0; j <= i; ++j) {
                res += product * state[vars[j]].get_value();
                product *= domains[vars[j]];
            }
            if (!fact_sets[i].count(res)) {
                return static_cast<int>(i) + 1;
            }
        } while (varsets.next());
    }
    return 0;
}


bool
NoveltyStore::insert(const State &state) {
    bool is_novel = false;
    for (unsigned i = 0; i < max_arity; ++i) {
        VarsetIterator varsets(domains.size(), i + 1);
        do {
            const auto &vars = *varsets;
            FactSetType res = offsets[i][varsets.get_idx()];
            FactSetType product = 1;
            for (unsigned j = 0; j <= i; ++j) {
                res += product * state[vars[j]].get_value();
                assert(vars[j] < domains.size());
                product *= domains[vars[j]];
            }
            auto inserted =
                fact_sets[i].insert(std::pair<FactSetType, int>(res, 1));
            if (inserted.second) {
                DMSG(
                    std::cout << "novel " << (i + 1) << "-fact-set: vars=[";
                    for (unsigned var = 0; var < vars.size(); ++var) {
                        std::cout << (var > 0 ? ", " : "") << vars[var];
                    }
                    std::cout << "] id=" << varsets.get_idx() << " hash=" << res << " values=[";
                    for (unsigned var = 0; var < vars.size(); ++var) {
                        std::cout << (var > 0 ? ", " : "") << state[vars[var]].get_name();
                    }
                    std::cout << "]" << std::endl;
                    )
                is_novel = true;
            } else {
                inserted.first->second = inserted.first->second + 1;
            }
        } while (varsets.next());
    }
    return is_novel;
}

bool
NoveltyStore::has_unique_factset(const State &state, unsigned arity) const {
    VarsetIterator varsets(domains.size(), arity);
    do {
        const auto &vars = *varsets;
        FactSetType res = offsets[arity - 1][varsets.get_idx()];
        FactSetType product = 1;
        for (unsigned j = 0; j < arity; ++j) {
            res += product * state[vars[j]].get_value();
            product *= domains[vars[j]];
        }
        auto it = fact_sets[arity - 1].find(res);
        if (it->second == 1) {
            return true;
        }
    } while (varsets.next());
    return false;
}

unsigned
NoveltyStore::size(unsigned arity) const {
    assert(arity > 0);
    assert(arity <= max_arity);
    return fact_sets[arity - 1].size();
}

unsigned
NoveltyStore::get_arity() const {
    return max_arity;
}

void
NoveltyStore::print_statistics() const {
    for (unsigned novelty = 1; novelty <= get_arity(); ++novelty) {
        std::cout << "Unique " << novelty << "-fact-sets: " << size(novelty) << std::endl;
    }
}
} // namespace policy_testing

#undef DMSG
