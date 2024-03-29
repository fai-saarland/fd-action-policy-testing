#pragma once

#include "shrink_bucket_based.h"

#include <vector>
#include <string>

/*
  TODO/NOTE: The behaviour of this strategy in the cases where we
  need to merge across buckets (i.e., when the number of (f, h) pairs
  is larger than the number of abstract states) is a bit stupid.

  In such a case, it would probably be better to at least try to be
  *h*-preserving, e.g. by switching to a strategy that buckets by h,
  giving high h the lowest priority.

  To see how we can get disastrous results here, try

  $ make debug && ./downward-1-debug --search 'astar(mas(10000,shrink_strategy=shrink_fh(LOW,HIGH)))' < output

  Of course, LOW/HIGH are not very clever parameters, but that is not
  the point here. The init h value drops from 43 to 0 at the point
  where we must merge across buckets, and never improves above 0 again.

  With the default HIGH/LOW parameter setting, we drop from 43 to 36
  (and then recover to 37), which is of course better, but still not
  great.
 */

namespace simulations {
class ShrinkFH : public ShrinkBucketBased {
public:
    enum class HighLow {
        HIGH, LOW
    };

private:
    const HighLow f_start;
    const HighLow h_start;

    void ordered_buckets_use_vector(
        const Abstraction &abs,
        std::vector<Bucket> &buckets) const;

    void ordered_buckets_use_map(
        const Abstraction &abs,
        std::vector<Bucket> &buckets) const;

protected:
    [[nodiscard]] std::string name() const override;

    void dump_strategy_specific_options() const override;

    void partition_into_buckets(const Abstraction &abs, std::vector<Bucket> &buckets) const override;

public:
    explicit ShrinkFH(const plugins::Options &opts);

    ~ShrinkFH() override = default;

    static ShrinkStrategy *create_default(int max_states);
};
}
