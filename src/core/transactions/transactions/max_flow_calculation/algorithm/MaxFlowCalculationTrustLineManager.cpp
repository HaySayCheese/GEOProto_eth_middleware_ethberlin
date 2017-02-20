//
// Created by mc on 19.02.17.
//

#include "MaxFlowCalculationTrustLineManager.h"

void MaxFlowCalculationTrustLineManager::addTrustLine(MaxFlowCalculationTrustLine *trustLine) {
    auto it = mvTrustLines.find(trustLine->getSourceUUID());
    if (it == mvTrustLines.end()) {
        vector<MaxFlowCalculationTrustLine::Shared> newVect;
        newVect.push_back(MaxFlowCalculationTrustLine::Shared(trustLine));
        mvTrustLines.insert(make_pair(trustLine->getSourceUUID(), newVect));
    } else {
        it->second.push_back(MaxFlowCalculationTrustLine::Shared(trustLine));
    }
}
