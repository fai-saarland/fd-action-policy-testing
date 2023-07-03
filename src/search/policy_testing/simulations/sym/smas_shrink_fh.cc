#include "smas_shrink_fh.h"

#include "sym_smas.h"

#include "../../../plugin.h"

#include <cassert>
#include <map>
#include <vector>

namespace simulations {
SMASShrinkFH::SMASShrinkFH(const Options &opts)
    : SMASShrinkBucketBased(opts),
      f_start(HighLow(opts.get<HighLow>("shrink_f"))),
      h_start(HighLow(opts.get<HighLow>("shrink_h"))) {
}

std::string SMASShrinkFH::name() const {
    return "f-preserving";
}

void SMASShrinkFH::dump_strategy_specific_options() const {
    std::cout << "Prefer shrinking high or low f states: "
              << (f_start == HighLow::HIGH ? "high" : "low") << std::endl
              << "Prefer shrinking high or low h states: "
              << (h_start == HighLow::HIGH ? "high" : "low") << std::endl;
}

void SMASShrinkFH::partition_into_buckets(
    const SymSMAS &abs, std::vector <Bucket> &buckets) const {
    assert(buckets.empty());
    // The following line converts to double to avoid overflow.
    int max_f = abs.get_max_f();
    if (static_cast<double>(max_f) * max_f / 2.0 > abs.size()) {
        // Use map because an average bucket in the vector structure
        // would contain less than 1 element (roughly).
        ordered_buckets_use_map(abs, buckets);
    } else {
        ordered_buckets_use_vector(abs, buckets);
    }
}

// Helper function for ordered_buckets_use_map.
template<class HIterator, class Bucket>
static void collect_h_buckets(
    HIterator begin, HIterator end,
    std::vector <Bucket> &buckets) {
    for (HIterator iter = begin; iter != end; ++iter) {
        Bucket &bucket = iter->second;
        assert(!bucket.empty());
        buckets.push_back(Bucket());
        buckets.back().swap(bucket);
    }
}

// Helper function for ordered_buckets_use_map.
template<class FHIterator, class Bucket>
static void collect_f_h_buckets(
    FHIterator begin, FHIterator end,
    SMASShrinkFH::HighLow h_start,
    std::vector <Bucket> &buckets) {
    for (FHIterator iter = begin; iter != end; ++iter) {
        if (h_start == SMASShrinkFH::HighLow::HIGH) {
            collect_h_buckets(iter->second.rbegin(), iter->second.rend(),
                              buckets);
        } else {
            collect_h_buckets(iter->second.begin(), iter->second.end(),
                              buckets);
        }
    }
}

void SMASShrinkFH::ordered_buckets_use_map(
    const SymSMAS &abs,
    std::vector <Bucket> &buckets) const {
    std::map<int, std::map<int, Bucket>> states_by_f_and_h;
    int bucket_count = 0;
    for (AbstractStateRef state = 0; state < abs.size(); ++state) {
        int g = abs.get_init_distance(state);
        int h = abs.get_goal_distance(state);
        if (g != PLUS_INFINITY && h != PLUS_INFINITY) {
            int f = g + h;
            Bucket &bucket = states_by_f_and_h[f][h];
            if (bucket.empty())
                ++bucket_count;
            bucket.push_back(state);
        }
    }

    buckets.reserve(bucket_count);
    if (f_start == HighLow::HIGH) {
        collect_f_h_buckets(
            states_by_f_and_h.rbegin(), states_by_f_and_h.rend(),
            h_start, buckets);
    } else {
        collect_f_h_buckets(
            states_by_f_and_h.begin(), states_by_f_and_h.end(),
            h_start, buckets);
    }
    assert(buckets.size() == bucket_count);
}

void SMASShrinkFH::ordered_buckets_use_vector(
    const SymSMAS &abs,
    std::vector <Bucket> &buckets) const {
    std::vector<std::vector<Bucket>> states_by_f_and_h;
    states_by_f_and_h.resize(abs.get_max_f() + 1);
    for (int f = 0; f <= abs.get_max_f(); ++f)
        states_by_f_and_h[f].resize(std::min(f, abs.get_max_h()) + 1);
    int bucket_count = 0;
    for (AbstractStateRef state = 0; state < abs.size(); ++state) {
        int g = abs.get_init_distance(state);
        int h = abs.get_goal_distance(state);
        if (g != PLUS_INFINITY && h != PLUS_INFINITY) {
            int f = g + h;
            assert(f >= 0 && f < states_by_f_and_h.size());
            assert(h >= 0 && h < states_by_f_and_h[f].size());
            Bucket &bucket = states_by_f_and_h[f][h];
            if (bucket.empty())
                ++bucket_count;
            bucket.push_back(state);
        }
    }

    buckets.reserve(bucket_count);
    int f_init = (f_start == HighLow::HIGH ? abs.get_max_f() : 0);
    int f_end = (f_start == HighLow::HIGH ? 0 : abs.get_max_f());
    int f_incr = (f_init > f_end ? -1 : 1);
    for (int f = f_init; f != f_end + f_incr; f += f_incr) {
        int h_init = (h_start == HighLow::HIGH ? states_by_f_and_h[f].size() - 1 : 0);
        int h_end = (h_start == HighLow::HIGH ? 0 : states_by_f_and_h[f].size() - 1);
        int h_incr = (h_init > h_end ? -1 : 1);
        for (int h = h_init; h != h_end + h_incr; h += h_incr) {
            Bucket &bucket = states_by_f_and_h[f][h];
            if (!bucket.empty()) {
                buckets.emplace_back();
                buckets.back().swap(bucket);
            }
        }
    }
    assert(buckets.size() == bucket_count);
}

SMASShrinkStrategy *SMASShrinkFH::create_default(int max_states) {
    options::Options opts;
    opts.set("max_states", max_states);
    opts.set("max_states_before_merge", max_states);
    opts.set("max_trs", PLUS_INFINITY);
    opts.set<HighLow>("shrink_f", SMASShrinkFH::HighLow::HIGH);
    opts.set<HighLow>("shrink_h", SMASShrinkFH::HighLow::LOW);
    return new SMASShrinkFH(opts);
}

static std::shared_ptr<SMASShrinkStrategy> _parse(options::OptionParser &parser) {
    SMASShrinkStrategy::add_options_to_parser(parser);
    std::vector<std::string> high_low {"HIGH", "LOW"};
    parser.add_enum_option<SMASShrinkFH::HighLow>(
        "shrink_f", high_low,
        "prefer shrinking states with high or low f values", "HIGH");
    parser.add_enum_option<SMASShrinkFH::HighLow>(
        "shrink_h", high_low, "prefer shrinking states with high or low h values", "LOW");
    Options opts = parser.parse();
    SMASShrinkStrategy::handle_option_defaults(opts);

    if (!parser.dry_run())
        return std::make_shared<SMASShrinkFH>(opts);
    else
        return nullptr;
}

static PluginTypePlugin<SMASShrinkStrategy> _plugin_type_smas_shrink_fh("smas_shrink_fh", "");
static Plugin <SMASShrinkStrategy> _plugin("smas_shrink_fh", _parse);
}
