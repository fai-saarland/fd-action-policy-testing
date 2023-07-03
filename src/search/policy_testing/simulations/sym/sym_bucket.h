#pragma once

#include "sym_util.h"

namespace simulations {
typedef std::vector<BDD> Bucket;

void removeZero(Bucket &bucket);

void copyBucket(const Bucket &bucket, Bucket &res);

void moveBucket(Bucket &bucket, Bucket &res);

int nodeCount(const Bucket &bucket);

bool extract_states(Bucket &list, const Bucket &pruned, Bucket &res);
}
