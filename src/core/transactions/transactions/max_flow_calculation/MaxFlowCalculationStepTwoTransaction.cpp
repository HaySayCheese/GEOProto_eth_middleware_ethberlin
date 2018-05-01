#include "MaxFlowCalculationStepTwoTransaction.h"

MaxFlowCalculationStepTwoTransaction::MaxFlowCalculationStepTwoTransaction(
    const NodeUUID &nodeUUID,
    const TransactionUUID &transactionUUID,
    const InitiateMaxFlowCalculationCommand::Shared command,
    TrustLinesManager *trustLinesManager,
    TopologyTrustLinesManager *topologyTrustLineManager,
    TopologyCacheManager *topologyCacheManager,
    MaxFlowCacheManager *maxFlowCacheManager,
    uint8_t maxFlowCalculationStep,
    Logger &logger) :

    BaseCollectTopologyTransaction(
        BaseTransaction::MaxFlowCalculationStepTwoTransactionType,
        transactionUUID,
        nodeUUID,
        command->equivalent(),
        trustLinesManager,
        topologyTrustLineManager,
        topologyCacheManager,
        maxFlowCacheManager,
        logger),
    mCommand(command),
    mMaxFlowCalculationStep(maxFlowCalculationStep)
{}

InitiateMaxFlowCalculationCommand::Shared MaxFlowCalculationStepTwoTransaction::command() const
{
    return mCommand;
}

TransactionResult::SharedConst MaxFlowCalculationStepTwoTransaction::sendRequestForCollectingTopology()
{
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "run\t" << "initiator: " << mNodeUUID;
    info() << "run\t" << "targets count: " << mCommand->contractors().size();
    info() << "SendRequestForCollectingTopology";
#endif

    mCountProcessCollectingTopologyRun = 0;
    return resultAwakeAfterMilliseconds(
        kWaitMillisecondsForCalculatingMaxFlow);
}

TransactionResult::SharedConst MaxFlowCalculationStepTwoTransaction::processCollectingTopology()
{
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "CalculateMaxTransactionFlow";
    info() << "context size: " << mContext.size();
#endif
    auto const contextSize = mContext.size();
    fillTopology();
    mCountProcessCollectingTopologyRun++;
    if (contextSize > 0 && mCountProcessCollectingTopologyRun <= kCountRunningProcessCollectingTopologyStage) {
        return resultAwakeAfterMilliseconds(
            kWaitMillisecondsForCalculatingMaxFlowAgain);
    }

    bool finalTopologyCollected = true;
    if (contextSize > 0){
        finalTopologyCollected = false;
    }

    vector<pair<NodeUUID, TrustLineAmount>> maxFlows;
    maxFlows.reserve(mCommand->contractors().size());
    mFirstLevelTopology =
            mTopologyTrustLineManager->trustLinePtrsSet(mNodeUUID);
    auto startTime = utc_now();
    for (const auto &contractorUUID : mCommand->contractors()) {
        auto nodeCache = mMaxFlowCacheManager->cacheByNode(contractorUUID);
        // todo : implement separated logic when !nodeCache->isFlowFinal()
        if (nodeCache != nullptr && nodeCache->isFlowFinal()) {
            maxFlows.push_back(
                make_pair(
                    contractorUUID,
                    nodeCache->currentFlow()));
        } else {
            maxFlows.push_back(
                make_pair(
                    contractorUUID,
                    calculateMaxFlow(
                        contractorUUID)));
        }
    }
    info() << "all contractors calculating time: " << (utc_now() - startTime);
    mTopologyTrustLineManager->setPreventDeleting(false);

    if (!finalTopologyCollected) {
        const auto kTransaction = make_shared<MaxFlowCalculationStepTwoTransaction>(
            mNodeUUID,
            currentTransactionUUID(),
            mCommand,
            mTrustLinesManager,
            mTopologyTrustLineManager,
            mTopologyCacheManager,
            mMaxFlowCacheManager,
            mMaxFlowCalculationStep + 1,
            mLog);
        launchSubsidiaryTransaction(kTransaction);
    }

    return resultOk(
        finalTopologyCollected,
        maxFlows);
}

// this method used the same logic as PathsManager::reBuildPaths
// and PathsManager::buildPaths
TrustLineAmount MaxFlowCalculationStepTwoTransaction::calculateMaxFlow(
    const NodeUUID &contractorUUID)
{
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "calculateMaxFlow\tstart found flow to: " << contractorUUID;
#endif
    DateTime startTime = utc_now();

    mTopologyTrustLineManager->makeFullyUsedTLsFromGatewaysToAllNodesExceptOne(
        contractorUUID);

    mCurrentContractor = contractorUUID;
    if (mFirstLevelTopology.empty()) {
        mTopologyTrustLineManager->resetAllUsedAmounts();
        return TrustLine::kZeroAmount();
    }

    mCurrentMaxFlow = TrustLine::kZeroAmount();
    for (mCurrentPathLength = 1; mCurrentPathLength <= kMaxPathLength; mCurrentPathLength++) {
        calculateMaxFlowOnOneLevel();
    }

    mTopologyTrustLineManager->resetAllUsedAmounts();
    info() << "max flow calculating time: " << utc_now() - startTime;
    mMaxFlowCacheManager->updateCache(
        mCurrentContractor,
        mCurrentMaxFlow,
        true);
    return mCurrentMaxFlow;
}

// this method used the same logic as PathsManager::reBuildPathsOnOneLevel
// and PathsManager::buildPathsOnOneLevel
void MaxFlowCalculationStepTwoTransaction::calculateMaxFlowOnOneLevel()
{
    while(true) {
        TrustLineAmount currentFlow = 0;
        for (auto &trustLinePtr : mFirstLevelTopology) {
            auto trustLine = trustLinePtr->topologyTrustLine();
            auto trustLineFreeAmountShared = trustLine->freeAmount();
            auto trustLineAmountPtr = trustLineFreeAmountShared.get();
            mForbiddenNodeUUIDs.clear();
            TrustLineAmount flow = calculateOneNode(
                trustLine->targetUUID(),
                *trustLineAmountPtr,
                1);
            if (flow > TrustLine::kZeroAmount()) {
                currentFlow += flow;
                trustLine->addUsedAmount(flow);
            }
        }
        if (currentFlow == 0) {
            break;
        }
    }
}

// it used the same logic as PathsManager::calculateOneNodeForRebuildingPaths
// and PathsManager::calculateOneNode
// if you change this method, you should change others
TrustLineAmount MaxFlowCalculationStepTwoTransaction::calculateOneNode(
    const NodeUUID& nodeUUID,
    const TrustLineAmount& currentFlow,
    byte level)
{
    if (nodeUUID == mCurrentContractor) {
        if (currentFlow > TrustLine::kZeroAmount()) {
            mCurrentMaxFlow += currentFlow;
        }
        return currentFlow;
    }
    if (level == mCurrentPathLength) {
        return 0;
    }

    auto trustLinePtrsSet =
            mTopologyTrustLineManager->trustLinePtrsSet(nodeUUID);
    if (trustLinePtrsSet.empty()) {
        return 0;
    }
    for (auto &trustLinePtr : trustLinePtrsSet) {
        auto trustLine = trustLinePtr->topologyTrustLine();
        if (trustLine->targetUUID() == mNodeUUID) {
            continue;
        }
        if (find(
                mForbiddenNodeUUIDs.begin(),
                mForbiddenNodeUUIDs.end(),
                trustLine->targetUUID()) != mForbiddenNodeUUIDs.end()) {
            continue;
        }
        TrustLineAmount nextFlow = currentFlow;
        auto trustLineFreeAmountShared = trustLine.get()->freeAmount();
        auto trustLineFreeAmountPtr = trustLineFreeAmountShared.get();
        if (*trustLineFreeAmountPtr < currentFlow) {
            nextFlow = *trustLineFreeAmountPtr;
        }
        if (nextFlow == TrustLine::kZeroAmount()) {
            continue;
        }
        mForbiddenNodeUUIDs.push_back(nodeUUID);
        TrustLineAmount calcFlow = calculateOneNode(
            trustLine->targetUUID(),
            nextFlow,
            level + (byte)1);
        mForbiddenNodeUUIDs.pop_back();
        if (calcFlow > TrustLine::kZeroAmount()) {
            trustLine->addUsedAmount(calcFlow);
            return calcFlow;
        }
    }
    return 0;
}

TransactionResult::SharedConst MaxFlowCalculationStepTwoTransaction::resultOk(
    bool finalMaxFlows,
    vector<pair<NodeUUID, TrustLineAmount>> &maxFlows)
{
    stringstream ss;
    if (finalMaxFlows) {
        ss << kFinalStep << kTokensSeparator << maxFlows.size();
    } else {
        ss << to_string(mMaxFlowCalculationStep) << kTokensSeparator << maxFlows.size();
    }
    for (const auto &nodeUUIDAndMaxFlow : maxFlows) {
        ss << kTokensSeparator << nodeUUIDAndMaxFlow.first << kTokensSeparator;
        ss << nodeUUIDAndMaxFlow.second;
    }
    auto kMaxFlowAmountsStr = ss.str();
    return transactionResultFromCommand(
        mCommand->responseOk(
            kMaxFlowAmountsStr));
}

const string MaxFlowCalculationStepTwoTransaction::logHeader() const
{
    stringstream s;
    s << "[MaxFlowCalculationStepTwoTA: " << currentTransactionUUID() << " " << mEquivalent << "]";
    return s.str();
}