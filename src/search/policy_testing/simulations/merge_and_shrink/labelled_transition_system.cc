#include "labelled_transition_system.h"

#include "abstraction.h"
#include "labels.h"

namespace simulations {
LabelledTransitionSystem::LabelledTransitionSystem(Abstraction *_abs, const LabelMap &labelMap) :
    abs(_abs), num_states(_abs->size()), goal_states(_abs->get_goal_states()) {
    int num_labels = labelMap.get_num_labels();
    const std::vector<bool> &was_rel_label = abs->get_relevant_labels();

    for (int i = 0; i < num_states; i++) {
        name_states.push_back(abs->description(i));
    }

    label_group_of_label.resize(num_labels, LabelGroup(-1));
    label_groups.reserve(num_labels);

    transitions_src.resize(abs->size());
    transitions_label_group.reserve(num_labels);

    for (int label_no = 0; label_no < num_labels; label_no++) {
        int old_label = labelMap.get_old_id(label_no);
        if (was_rel_label[old_label]) {
            const std::vector <AbstractTransition> &abs_transitions = abs->get_transitions_for_label(old_label);

            if (!abs_transitions.empty()) {
                std::vector <TSTransition> transitions_label;
                relevant_labels.push_back(label_no);
                transitions_label.reserve(abs_transitions.size());
                for (const auto &abs_transition : abs_transitions) {
                    transitions_label.emplace_back(abs_transition.src, abs_transition.target);
                }
                std::sort(transitions_label.begin(), transitions_label.end());

                for (int g = 0; g < transitions_label_group.size(); ++g) {
                    if (transitions_label_group[g] == transitions_label) {
                        assert(g < label_groups.size());
                        label_groups[g].push_back(label_no);
                        label_group_of_label[label_no] = LabelGroup(g);
                        break;
                    }
                }
                if (label_group_of_label[label_no].dead()) {
                    LabelGroup new_group(transitions_label_group.size());
                    for (const TSTransition &tr: transitions_label) {
                        transitions.emplace_back(tr.src, tr.target, new_group);
                        transitions_src[tr.src].emplace_back(tr.src, tr.target, new_group);
                    }
                    transitions_label_group.push_back(std::move(transitions_label));
                    label_groups.emplace_back();
                    label_groups[new_group.group].push_back(label_no);
                    label_group_of_label[label_no] = new_group;
                }
            } else {
                //dead_label
            }
        } else {
            irrelevant_labels.push_back(label_no);
        }
    }
}

void LabelledTransitionSystem::kill_transition(int src, int label, int target) {
    auto group = label_group_of_label[label];

    if (label_groups[group.group].size() == 1) {
        LTSTransition t(src, target, LabelGroup(group));
        kill_from_vector(t, transitions);
        kill_from_vector(t, transitions_src[src]);
        kill_from_vector(TSTransition(src, target), transitions_label_group[group.group]);
    } else {
        LabelGroup new_group(transitions_label_group.size());
        transitions_label_group.push_back(transitions_label_group[group.group]);
        kill_from_vector(TSTransition(src, target), transitions_label_group[new_group.group]);
        for (const auto &t: transitions_label_group[new_group.group]) {
            transitions.emplace_back(t.src, t.target, new_group);
            transitions_src[t.src].emplace_back(t.src, t.target, new_group);
        }
        label_groups[group.group].erase(remove(std::begin(label_groups[group.group]),
                                               std::end(label_groups[group.group]), label),
                                        std::end(label_groups[group.group]));
        label_groups[new_group.group].push_back(label);
        label_group_of_label[label] = new_group;
    }
}


void LabelledTransitionSystem::kill_label(int l) {
    std::cout << "KILL " << l << std::endl;
    auto group = label_group_of_label[l];
    if (group.dead()) {
        irrelevant_labels.erase(remove(std::begin(irrelevant_labels), std::end(irrelevant_labels), l),
                                std::end(irrelevant_labels));
    } else {
        label_group_of_label[l] = LabelGroup(-1);

        relevant_labels.erase(remove(std::begin(relevant_labels), std::end(relevant_labels), l),
                              std::end(relevant_labels));
        label_groups[group.group].erase(remove(std::begin(label_groups[group.group]),
                                               std::end(label_groups[group.group]), l),
                                        std::end(label_groups[group.group]));
        if (label_groups[group.group].empty()) {
            //Kill group
            std::vector<TSTransition>().swap(transitions_label_group[l]);

            transitions.erase(std::remove_if(std::begin(transitions),
                                             std::end(transitions),
                                             [&](LTSTransition &t) {
                                                 return t.label_group == group;
                                             }), std::end(transitions));

            for (auto &trs: transitions_src) {
                trs.erase(std::remove_if(std::begin(trs),
                                         std::end(trs),
                                         [&](LTSTransition &t) {
                                             return t.label_group == group;
                                         }), std::end(trs));
            }
        }
    }
}


void LabelledTransitionSystem::dump() const {
    for (int s = 0; s < size(); s++) {
        applyPostSrc(s, [&](const LTSTransition &trs) {
                         std::cout << trs.src << " -> " << trs.target << " (" << trs.label_group.group << ":";
                         for (int tr_s_label: get_labels(trs.label_group)) {
                             std::cout << " " << tr_s_label;
                         }
                         std::cout << ")\n";
                         return false;
                     });
    }
}

bool LabelledTransitionSystem::is_self_loop_everywhere_label(int label) const {
    if (!is_relevant_label(label))
        return true;
    const auto &trs = get_transitions_label(label);
    if (trs.size() < num_states)
        return false;

    // This assumes that there is no repeated transition
    int num_self_loops = 0;
    for (const auto &tr : trs) {
        if (tr.src == tr.target) {
            num_self_loops++;
        }
    }

    assert(num_self_loops <= num_states);
    return num_self_loops == num_states;
}
}
