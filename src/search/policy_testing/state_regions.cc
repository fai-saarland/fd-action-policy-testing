#include "state_regions.h"

#include "../task_utils/successor_generator.h"
#include "oracle.h"

#include <algorithm>

namespace policy_testing {
template<typename StateContainer>
StateRegions
compute_state_regions(
    const std::shared_ptr<AbstractTask> &task,
    StateRegistry &state_registry,
    const StateContainer &state_ids) {
    constexpr const bool is_map_t = std::is_same_v<StateContainer, utils::HashMap<StateID, TestResult>>;

    StateRegions regions;
    utils::HashMap<StateID, unsigned> state_to_region;
    regions.resize(state_ids.size());
    std::vector<unsigned> empty_bins;
    {
        unsigned i = 0;
        if constexpr (is_map_t) {
            for (auto [state_id, _]: state_ids) {
                state_to_region[state_id] = i;
                regions[i].push_back(state_id);
                ++i;
            }
        } else {
            for (StateID state_id: state_ids) {
                state_to_region[state_id] = i;
                regions[i].push_back(state_id);
                ++i;
            }
        }
    }
    successor_generator::SuccessorGenerator &succ_gen =
        successor_generator::g_successor_generators[TaskProxy(*task)];
    TaskProxy task_proxy(*task);
    std::vector<OperatorID> aops;
    if constexpr (is_map_t) {
        for (auto [state_id, _] : state_ids) {
            const unsigned region_idx = state_to_region[state_id];
            std::vector<StateID> &r = regions[region_idx];
            State state = state_registry.lookup_state(state_id);
            succ_gen.generate_applicable_ops(state, aops);
            for (auto &aop : aops) {
                State succ = state_registry.get_successor_state(
                    state, task_proxy.get_operators()[aop]);
                auto x = state_to_region.find(succ.get_id());
                if (x != state_to_region.end() && x->second != region_idx) {
                    empty_bins.push_back(x->second);
                    std::vector<StateID> &alt_r = regions[x->second];
                    r.insert(r.end(), alt_r.begin(), alt_r.end());
                    for (StateID other_id : alt_r) {
                        state_to_region[other_id] = region_idx;
                    }
                    alt_r.clear();
                }
            }
            aops.clear();
        }
    } else {
        for (StateID state_id : state_ids) {
            const unsigned region_idx = state_to_region[state_id];
            std::vector<StateID> &r = regions[region_idx];
            State state = state_registry.lookup_state(state_id);
            succ_gen.generate_applicable_ops(state, aops);
            for (auto &aop : aops) {
                State succ = state_registry.get_successor_state(
                    state, task_proxy.get_operators()[aop]);
                auto x = state_to_region.find(succ.get_id());
                if (x != state_to_region.end() && x->second != region_idx) {
                    empty_bins.push_back(x->second);
                    std::vector<StateID> &alt_r = regions[x->second];
                    r.insert(r.end(), alt_r.begin(), alt_r.end());
                    for (StateID other_id : alt_r) {
                        state_to_region[other_id] = region_idx;
                    }
                    alt_r.clear();
                }
            }
            aops.clear();
        }
    }
    if (!empty_bins.empty()) {
        std::sort(empty_bins.begin(), empty_bins.end());
        unsigned j = empty_bins[0];
        unsigned k = 1;
        for (unsigned i = empty_bins[0] + 1; i < regions.size(); ++i) {
            if (k < empty_bins.size() && i == empty_bins[k]) {
                ++k;
                continue;
            }
            std::swap(regions[j], regions[i]);
            ++j;
        }
        regions.resize(j);
        regions.shrink_to_fit();
    }
    return regions;
}

template
StateRegions compute_state_regions(
    const std::shared_ptr<AbstractTask> &task,
    StateRegistry &state_registry,
    const utils::HashMap<StateID, TestResult> &state_ids);

template
StateRegions compute_state_regions(
    const std::shared_ptr<AbstractTask> &task,
    StateRegistry &state_registry,
    const utils::HashSet<StateID> &state_ids);
} // namespace policy_testing
