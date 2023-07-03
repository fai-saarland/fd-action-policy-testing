#pragma once

#include "../abstract_task.h"
#include "../state_id.h"
#include "../state_registry.h"
#include "../utils/hash.h"

#include <vector>

namespace policy_testing {
using StateRegion = std::vector<StateID>;
using StateRegions = std::vector<StateRegion>;

template<typename StateContainer>
StateRegions compute_state_regions(
    const std::shared_ptr<AbstractTask> &task,
    StateRegistry &state_registry,
    const StateContainer &state_ids);
} // namespace policy_testing
