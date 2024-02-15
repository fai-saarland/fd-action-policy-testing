#pragma once

#include "../simulations_manager.h"

#include <vector>
#include <set>

namespace simulations {
class CompositeLabel;
class Abstraction;

/* This class implements labels as used by merge-and-shrink abstractions.
   It abstracts from the underlying regular operators and allows to store
   additional information associated with labels.
   NOTE: operators that are axioms are currently not supported! */

class Label {
    friend class CompositeLabel;     // required for update_root
    int id;
    int cost;
    // prevail and pre_posts are references to those of one "canonical"
    // operator, which is the operator an OperatorLabel was built from or
    // the "first" label of all parent labels when constructing a CompositeLabel.
    const std::vector<Prevail> &prevail;
    const std::vector<PrePost> &pre_post;

    //Alvaro: Sets of abstraction the label is relevant for. This is
    //needed to compute the own-labels for an abstraction (those that
    //are only relevant for the abstraction).
    std::set<Abstraction *> relevant_for;
protected:
    // root is a pointer to a composite label that this label has been reduced
    // to, if such a label exists, or to itself, if the label has not been
    // reduced yet.
    Label *root;

    Label(int id, int cost, const std::vector<Prevail> &prevail,
          const std::vector<PrePost> &pre_post);

    virtual ~Label() = default;

    virtual void update_root(CompositeLabel *new_root) = 0;

public:
    [[nodiscard]] const std::vector<Prevail> &get_prevail() const {return prevail;}

    [[nodiscard]] const std::vector<PrePost> &get_pre_post() const {return pre_post;}

    [[nodiscard]] int get_id() const {
        return id;
    }

    [[nodiscard]] int get_cost() const {
        return cost;
    }

    [[nodiscard]] bool is_reduced() const;

    [[nodiscard]] virtual const std::vector<Label *> &get_parents() const = 0;

    void dump() const;

    //Alvaro: Methods to access relevant_for.
    void set_relevant_for(Abstraction *abstraction);

    void set_irrelevant_for(Abstraction *abstraction);

    bool is_relevant_for(Abstraction *abstraction) const;

    void reset_relevant_for(const std::vector<Abstraction *> &abstractions);


    [[nodiscard]] const std::set<Abstraction *> &get_relevant_for() const {
        return relevant_for;
    }

    [[nodiscard]] bool is_irrelevant() const;

    virtual void get_operators(std::set<int> &ops_id) const = 0;
};

class OperatorLabel : public Label {
    void update_root(CompositeLabel *new_root) override;

public:
    OperatorLabel(int id, int cost, const std::vector<Prevail> &prevail,
                  const std::vector<PrePost> &pre_post);

    [[nodiscard]] const std::vector<Label *> &get_parents() const override;

    void get_operators(std::set<int> &ops_id) const override {
        ops_id.insert(get_id());
    }
};

class CompositeLabel : public Label {
    std::vector<Label *> parents;

    void update_root(CompositeLabel *new_root) override;

public:
    CompositeLabel(int id, const std::vector<Label *> &parents);

    [[nodiscard]] const std::vector<Label *> &get_parents() const override {
        return parents;
    }

    void get_operators(std::set<int> &ops_id) const override {
        for (auto p: parents) {
            p->get_operators(ops_id);
        }
    }
};
}
