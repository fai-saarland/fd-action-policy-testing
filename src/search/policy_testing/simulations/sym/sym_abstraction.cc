#include "sym_abstraction.h"

#include "sym_transition.h"
#include "sym_util.h"
#include "../utils/debug.h"

namespace simulations {
std::ostream &operator<<(std::ostream &os, const SymAbstraction &abs) {
    abs.print(os, false);
    return os;
}

void SymAbstraction::shrinkTransitions(const std::map<int, std::vector<SymTransition>> &trs,
                                       const std::map<int, std::vector<SymTransition>> &indTRs,
                                       std::map<int, std::vector<SymTransition>> &res,
                                       int maxTime, int maxNodes) const {
    std::map<int, std::vector<SymTransition>> failedToShrink;

    switch (absTRsStrategy) {
    case AbsTRsStrategy::TR_SHRINK:
        for (const auto &trsParent: trs) {
            int cost = trsParent.first;         //For all the TRs of cost of variable cost
            std::cout << "Init trs: " << cost << std::endl;
            std::set<OperatorID> failed_ops;

            int num_trs = trs.size() * trsParent.second.size();
            for (const auto &trParent: trsParent.second) {
                SymTransition absTransition = SymTransition(trParent);
                DEBUG_MSG(std::cout << "SHRINK: " << absTransition << " time_out: "
                                    << 1 + maxTime / num_trs << " max nodes: "
                                    << 1 + maxNodes << std::endl;
                          );
                try {
                    vars->setTimeLimit(1 + maxTime / num_trs);
                    absTransition.shrink(*this, 1 + maxNodes);
                    res[cost].push_back(std::move(absTransition));
                    vars->unsetTimeLimit();
                } catch (BDDError &e) {
                    vars->unsetTimeLimit();
                    DEBUG_MSG(std::cout << "Failed shrinking TR" << std::endl;
                              );
                    //Failed some
                    const std::set<OperatorID> &tr_ops = trParent.getOps();
                    failed_ops.insert(begin(tr_ops), end(tr_ops));
                }
            }

            if (!failed_ops.empty()) {         //Add all the TRs related with it.
                std::cout << "Failed ops" << std::endl;
                for (const auto &trParent: indTRs.at(cost)) {
                    if (trParent.hasOp(failed_ops)) {
                        SymTransition absTransition = SymTransition(trParent);
                        vars->setTimeLimit(maxTime);
                        try {
                            absTransition.shrink(*this, maxNodes);
                            res[cost].push_back(absTransition);
                            vars->unsetTimeLimit();
                        } catch (BDDError &e) {
                            failedToShrink[cost].push_back(absTransition);
                            vars->unsetTimeLimit();
                        }
                    }
                }
            }
            merge(vars, res[cost], mergeTR, maxTime / trs.size(), maxNodes / trs.size());
        }
        break;
    case AbsTRsStrategy::IND_TR_SHRINK:
        for (const auto &indTRsCost: indTRs) {
            int cost = indTRsCost.first;
            for (const auto &trParent: indTRsCost.second) {
                SymTransition absTransition = SymTransition(trParent);
                try {
                    vars->setTimeLimit(maxTime);
                    absTransition.shrink(*this, maxNodes);
                    vars->unsetTimeLimit();
                    res[cost].push_back(absTransition);
                } catch (BDDError &e) {
                    vars->unsetTimeLimit();
                    failedToShrink[cost].push_back(absTransition);
                }
            }
            merge(vars, res[cost], mergeTR, maxTime, maxNodes);
        }
        break;
    case AbsTRsStrategy::REBUILD_TRS:
        return getTransitions(indTRs, res);
    case AbsTRsStrategy::SHRINK_AFTER_IMG:
        //SetAbsAfterImage
        for (const auto &t: trs) {
            int cost = t.first;
            for (const auto &tr: t.second) {
                SymTransition newTR = tr;
                newTR.setAbsAfterImage(this);
                res[cost].push_back(newTR);
            }
        }
        break;
    }

    //Use Shrink after img in all the transitions that failedToShrink
    DEBUG_MSG(std::cout << "Failed to shrink: " << (failedToShrink.empty() ? "no" : "yes") << std::endl;
              );
    for (auto &failedTRs: failedToShrink) {
        merge(vars, failedTRs.second, mergeTR, maxTime, maxNodes);
        for (auto &tr: failedTRs.second) {
            tr.setAbsAfterImage(this);
            res[failedTRs.first].push_back(tr);
        }
    }
}
}
