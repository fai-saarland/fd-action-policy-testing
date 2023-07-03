#include "merge_criterion.h"

#include "../../../task_utils/causal_graph.h"
#include "../simulations_manager.h"
#include "abstraction.h"
#include "../plugin.h"
#include <vector>
#include <map>

namespace simulations {
void MergeCriterionCG::init() {
    is_causal_predecessor.resize(global_simulation_task()->get_num_variables(), false);
}

void MergeCriterionCG::select_next(int var_no) {
    if (allow_incremental) {
        const std::vector<int> &new_vars = global_simulation_task_proxy()->get_causal_graph().get_eff_to_pre(var_no);
        for (int new_var : new_vars)
            is_causal_predecessor[new_var] = true;
    }
}

void MergeCriterionCG::filter(const std::vector<Abstraction *> & /*all_abstractions*/,
                              std::vector<int> &vars, Abstraction *abstraction) {
    if (!abstraction)
        return;
    if (!allow_incremental) {
        is_causal_predecessor.resize(global_simulation_task()->get_num_variables(), false);
        const std::vector<int> &varset = abstraction->get_varset();
        const auto &causal_graph = global_simulation_task_proxy()->get_causal_graph();
        for (int v : varset) {
            const std::vector<int> &new_vars = causal_graph.get_eff_to_pre(v);
            for (int new_var : new_vars) {
                is_causal_predecessor[new_var] = true;
            }
        }
    }

    MergeCriterion::filter(vars, is_causal_predecessor);
}


void MergeCriterionGoal::init() {
    is_goal_variable.resize(global_simulation_task()->get_num_variables(), false);
    for (int i = 0; i < global_simulation_task()->get_num_goals(); ++i) {
        is_goal_variable[global_simulation_task()->get_goal_fact(i).var] = true;
    }
}

void MergeCriterionGoal::select_next(int /*var_no*/) {}

void MergeCriterionGoal::filter(const std::vector<Abstraction *> & /*all_abstractions*/,
                                std::vector<int> &vars, Abstraction * /*abstraction*/) {
    MergeCriterion::filter(vars, is_goal_variable);
}

void MergeCriterionMinSCC::init() {
    is_causal_predecessor.resize(global_simulation_task()->get_num_variables(), false);
    const auto &causal_graph = global_simulation_task_proxy()->get_causal_graph();
    const std::vector<std::vector<int>> &graph =
        (complete_cg ? (reverse ? causal_graph.get_predecessors() : causal_graph.get_successors()) :
         (reverse ? causal_graph.get_eff_to_pre() : causal_graph.get_pre_to_eff()));

    scc = std::make_unique<SCC>(graph);
}

void MergeCriterionMinSCC::select_next(int var_no) {
    if (allow_incremental) {
        const std::vector<int> &new_vars =
            global_simulation_task_proxy()->get_causal_graph().get_eff_to_pre(var_no);
        for (int new_var : new_vars)
            is_causal_predecessor[new_var] = true;
    }
}

void MergeCriterionMinSCC::forbid_scc_descendants(int scc_index,
                                                  const std::vector <std::set<int>> &scc_graph,
                                                  std::vector<bool> &forbidden_sccs) const {
    const std::set<int> &descendants = scc_graph[scc_index];
    for (int descendant : descendants) {
        if (!forbidden_sccs[descendant]) {
            forbidden_sccs[descendant] = true;
            forbid_scc_descendants(descendant, scc_graph, forbidden_sccs);
        }
    }
}

void MergeCriterionMinSCC::filter(const std::vector<Abstraction *> & /*all_abstractions*/,
                                  std::vector<int> &vars,
                                  Abstraction *abstraction) {
    if (!abstraction)
        return;
    if (!allow_incremental) {
        is_causal_predecessor.resize(global_simulation_task()->get_num_variables(), false);
        const std::vector<int> &varset = abstraction->get_varset();
        const auto &causal_graph = global_simulation_task_proxy()->get_causal_graph();
        for (int v : varset) {
            const std::vector<int> &new_vars = causal_graph.get_eff_to_pre(v);
            for (int new_var : new_vars) {
                is_causal_predecessor[new_var] = true;
            }
        }
    }

    if (!MergeCriterion::filter(vars, is_causal_predecessor)) {
        return;     //No CG relevant vars => we do not prefer any variable over another
    }

    const std::vector<std::set<int>> &scc_graph = scc->get_scc_graph();
    const std::vector<int> &vars_scc = scc->get_vertex_scc();
    std::vector<bool> forbidden_sccs(scc_graph.size(), false);
    //In each SCC,select only the variable whose "level" is "closer to the root"
    //We consider a variable closer to the root if it has lower id
    //If reverse is activated, we consider the opposite order
    std::map<int, int> vars_by_scc;
    //1) forbid all sccs pointed by scc_var
    for (int var : vars) {
        int scc_var = vars_scc[var];
        if (!forbidden_sccs[scc_var]) {
            forbid_scc_descendants(scc_var, scc_graph, forbidden_sccs);
            if (!vars_by_scc.count(scc_var) || (!reverse && vars_by_scc[scc_var] > var) ||
                (reverse && vars_by_scc[scc_var] < var)) {
                vars_by_scc[scc_var] = var;
            }
        }
    }

    //2) Filter all variables whose scc has been forbidden.
    std::vector<int> new_vars;
    if (tie_by_level) {     //For valid sccs, include the selected variable
        for (auto &it : vars_by_scc) {
            if (!forbidden_sccs[it.first]) {
                new_vars.push_back(it.second);
            }
        }
    } else {
        //For valid sccs, include all variables
        for (int var : vars) {
            int scc_var = vars_scc[var];
            if (!forbidden_sccs[scc_var])
                new_vars.push_back(var);
        }
    }
    vars.swap(new_vars);
}


MergeCriterionTRs::MergeCriterionTRs(const Options &opts) :
    only_goals(opts.get<bool>("goal")),
    only_empty(opts.get<bool>("empty")),
    opt_factor(opts.get<double>("opt_factor")),
    opt_diff(opts.get<int>("opt_diff")) {
}


void MergeCriterionTRs::init() {}

void MergeCriterionTRs::select_next(int /*var_no*/) {}

void MergeCriterionTRs::filter(const std::vector<Abstraction *> &all_abstractions,
                               std::vector<int> &vars, Abstraction *abstraction) {
    if (abstraction) {     //Check if abstraction exists, because the first variable is selected without abstraction
        std::vector<int> score;
        abstraction->normalize();
        abstraction->count_transitions(all_abstractions, vars,
                                       only_empty, only_goals, score);
        //for(int i = 0; i < vars.size(); i++)
        //    cout << "ScoreTRs(" << only_empty << ", " << only_goals << ") " << vars[i] << ": " << score[vars[i]] << " " << endl;
        std::cout << "TRS criterion: " << vars.size();
        MergeCriterion::filter_best(vars, score, false, opt_factor, opt_diff);
        std::cout << " => " << vars.size() << std::endl;
    }
}

void MergeCriterionRelevant::init() {
    MergeCriterionCG::init();
    for (int i = 0; i < global_simulation_task()->get_num_goals(); ++i) {
        is_causal_predecessor[global_simulation_task()->get_goal_fact(i).var] = true;
    }
}

template<class T>
static std::shared_ptr<MergeCriterion> _parse(OptionParser &parser) {
    if (parser.dry_run()) {
        return nullptr;
    } else {
        return std::make_shared<T>();
    }
}

//TODO: This should be removed, because it is equivalent to scc(level=false)
static std::shared_ptr<MergeCriterion> _parse_scc_no_level(options::OptionParser &parser) {
    if (parser.dry_run()) {
        return nullptr;
    } else {
        return std::make_shared<MergeCriterionMinSCC>(false, false);
    }
}


MergeCriterionMinSCC::MergeCriterionMinSCC(const options::Options &opts) :
    reverse(opts.get<bool>("reverse")),
    tie_by_level(opts.get<bool>("level")),
    complete_cg(opts.get<bool>("complete_cg")) {
}

static std::shared_ptr<MergeCriterion> _parse_scc(options::OptionParser &parser) {
    parser.add_option<bool>("reverse", "reverse scc criterion", "false");
    parser.add_option<bool>("level", "use level or not in the scc criterion", "false");
    parser.add_option<bool>("complete_cg", "use the old or the new cg", "false");
    Options opts = parser.parse();

    if (parser.dry_run()) {
        return nullptr;
    } else {
        return std::make_shared<MergeCriterionMinSCC>(opts);
    }
}

static std::shared_ptr<MergeCriterion> _parse_tr(OptionParser &parser) {
    parser.add_option<bool>("goal", "only counts transitions leading to a goal state", "false");
    parser.add_option<bool>("empty", "only counts transitions that will become empty", "false");
    parser.add_option<double>("opt_factor",
                              "allows for a multiplicative factor of suboptimality in the number of TRs", "1.0");
    parser.add_option<int>("opt_diff", "allows for a constant factor of suboptimality in the number of TRs", "0");
    Options opts = parser.parse();

    if (parser.dry_run()) {
        return nullptr;
    } else {
        return std::make_shared<MergeCriterionTRs>(opts);
    }
}

static PluginTypePlugin<MergeCriterion> _plugin_type_merge_criterion("MergeCriterion", "");
static Plugin<MergeCriterion> _plugin_cg("cg", _parse<MergeCriterionCG>);
static Plugin<MergeCriterion> _plugin_goal("goal", _parse<MergeCriterionGoal>);
static Plugin<MergeCriterion> _plugin_scc("scc", _parse_scc);
static Plugin<MergeCriterion> _plugin_scc_no_level("scc_no_level", _parse_scc_no_level);
static Plugin<MergeCriterion> _plugin_tr("tr", _parse_tr);
static Plugin<MergeCriterion> _plugin_rel("relevant", _parse<MergeCriterionRelevant>);
}
