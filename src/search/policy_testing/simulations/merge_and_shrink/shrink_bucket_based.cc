#include "shrink_bucket_based.h"

#include "abstraction.h"

#include "../simulations_manager.h"

#include <cassert>
#include <iostream>
#include <vector>

namespace simulations {
ShrinkBucketBased::ShrinkBucketBased(const Options &opts)
    : ShrinkStrategy(opts) {
}

bool ShrinkBucketBased::reduce_labels_before_shrinking() const {
    return false;
}

void ShrinkBucketBased::shrink(Abstraction &abs, int threshold, bool force) {
    if (must_shrink(abs, threshold, force)) {
        std::vector <Bucket> buckets;
        partition_into_buckets(abs, buckets);

        EquivalenceRelation equiv_relation;
        compute_abstraction(buckets, threshold, equiv_relation);
        apply(abs, equiv_relation, threshold);
    }
}

void ShrinkBucketBased::compute_abstraction(
    const std::vector<Bucket> &buckets, int target_size,
    EquivalenceRelation &equiv_relation) const {
    bool show_combine_buckets_warning = true;

    assert(equiv_relation.empty());
    equiv_relation.reserve(target_size);

    size_t num_states_to_go = 0;
    for (const auto &bucket : buckets)
        num_states_to_go += bucket.size();

    for (size_t bucket_no = 0; bucket_no < buckets.size(); ++bucket_no) {
        const std::vector <AbstractStateRef> &bucket = buckets[bucket_no];
        int states_used_up = static_cast<int>(equiv_relation.size());
        int remaining_state_budget = target_size - states_used_up;
        num_states_to_go -= bucket.size();
        int budget_for_this_bucket = remaining_state_budget - num_states_to_go;

        if (budget_for_this_bucket >= static_cast<int>(bucket.size())) {
            // Each state in bucket can become a singleton group.
            for (int i : bucket) {
                EquivalenceClass group;
                group.push_front(i);
                equiv_relation.push_back(group);
            }
        } else if (budget_for_this_bucket <= 1) {
            // The whole bucket must form one group.
            size_t remaining_buckets = buckets.size() - bucket_no;
            if (remaining_state_budget >= remaining_buckets) {
                equiv_relation.push_back(EquivalenceClass());
            } else {
                if (bucket_no == 0)
                    equiv_relation.push_back(EquivalenceClass());
                if (show_combine_buckets_warning) {
                    show_combine_buckets_warning = false;
                    std::cout << "Very small node limit, must combine buckets." << std::endl;
                }
            }
            EquivalenceClass &group = equiv_relation.back();
            group.insert(group.begin(), bucket.begin(), bucket.end());
        } else {
            // Complicated case: must combine until bucket budget is met.
            // First create singleton groups.
            std::vector <EquivalenceClass> groups(bucket.size());
            for (size_t i = 0; i < bucket.size(); ++i)
                groups[i].push_front(bucket[i]);

            // Then combine groups until required size is reached.
            assert(budget_for_this_bucket >= 2 &&
                   budget_for_this_bucket < groups.size());
            while (groups.size() > budget_for_this_bucket) {
                size_t pos1 = simulations_rng(groups.size());
                size_t pos2;
                do {
                    pos2 = simulations_rng(groups.size());
                } while (pos1 == pos2);
                groups[pos1].splice(groups[pos1].begin(), groups[pos2]);
                swap(groups[pos2], groups.back());
                assert(groups.back().empty());
                groups.pop_back();
            }

            // Finally, add these groups to the result.
            for (auto &group : groups) {
                equiv_relation.push_back(EquivalenceClass());
                equiv_relation.back().swap(group);
            }
        }
    }
}
}
