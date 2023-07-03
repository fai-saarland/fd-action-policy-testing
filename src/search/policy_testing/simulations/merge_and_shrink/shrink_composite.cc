#include "shrink_composite.h"

#include <utility>

#include "abstraction.h"

#include "../../../plugin.h"

namespace simulations {
ShrinkComposite::ShrinkComposite(const Options &opts)
    : ShrinkStrategy(opts), strategies(opts.get_list<std::shared_ptr<ShrinkStrategy>>("strategies")) {
}

ShrinkComposite::ShrinkComposite(const Options &opts, std::vector<std::shared_ptr<ShrinkStrategy>> sts)
    : ShrinkStrategy(opts), strategies(std::move(sts)) {
}

std::string ShrinkComposite::name() const {
    return "composite";
}

void ShrinkComposite::dump_strategy_specific_options() const {
    for (const auto &strategy : strategies) {
        strategy->dump_options();
    }
}

bool ShrinkComposite::reduce_labels_before_shrinking() const {
    for (const auto &strategy : strategies) {
        if (strategy->reduce_labels_before_shrinking()) {
            return true;
        }
    }
    return false;
}

void ShrinkComposite::shrink(Abstraction &abs, int target, bool force) {
    //TODO: This method does not make much sense in here. Should it be
    //removed from shrink_strategy?
    for (int i = 0; i < strategies.size(); ++i) {
        if (i != 0) {
            abs.compute_distances();
            abs.normalize();
            assert(abs.is_solvable());
        }
        strategies[i]->shrink(abs, target, force);
    }
}

void ShrinkComposite::shrink_atomic(Abstraction &abs) {
    for (int i = 0; i < strategies.size(); ++i) {
        if (i != 0) {
            abs.normalize();
            abs.compute_distances();
        }
        strategies[i]->shrink_atomic(abs);
    }
}

void ShrinkComposite::shrink_before_merge(
    Abstraction &abs1, Abstraction &abs2) {
    for (int i = 0; i < strategies.size(); ++i) {
        if (i != 0) {
            abs1.normalize();
            abs2.normalize();
            abs1.compute_distances();
            abs2.compute_distances();
        }
        strategies[i]->shrink_before_merge(abs1, abs2);
    }
}


std::shared_ptr<ShrinkComposite> ShrinkComposite::create_default(
    const std::vector<std::shared_ptr<ShrinkStrategy>> &sts) {
    Options opts;
    opts.set("max_states", PLUS_INFINITY);
    opts.set("max_states_before_merge", PLUS_INFINITY);

    return std::make_shared<ShrinkComposite>(opts, sts);
}


static std::shared_ptr<ShrinkStrategy> _parse(options::OptionParser &parser) {
    ShrinkStrategy::add_options_to_parser(parser);
    parser.add_list_option<std::shared_ptr<ShrinkStrategy>>("strategies");

    Options opts = parser.parse();

    if (parser.help_mode())
        return nullptr;

    ShrinkStrategy::handle_option_defaults(opts);

    opts.verify_list_non_empty<std::shared_ptr<ShrinkStrategy>>("strategies");

    if (!parser.dry_run())
        return std::make_shared<ShrinkComposite>(opts);
    else
        return nullptr;
}

static Plugin <ShrinkStrategy> _plugin("shrink_composite", _parse);
}
