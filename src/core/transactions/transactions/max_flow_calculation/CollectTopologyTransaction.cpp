#include "CollectTopologyTransaction.h"

CollectTopologyTransaction::CollectTopologyTransaction(
    const NodeUUID &nodeUUID,
    vector<NodeUUID> &contractors,
    TrustLinesManager *manager,
    MaxFlowCalculationTrustLineManager *maxFlowCalculationTrustLineManager,
    MaxFlowCalculationCacheManager *maxFlowCalculationCacheManager,
    Logger &logger) :

    BaseTransaction(
        BaseTransaction::TransactionType::CollectTopologyTransactionType,
        nodeUUID,
        logger),
    mContractors(contractors),
    mTrustLinesManager(manager),
    mMaxFlowCalculationTrustLineManager(maxFlowCalculationTrustLineManager),
    mMaxFlowCalculationCacheManager(maxFlowCalculationCacheManager)
{}

TransactionResult::SharedConst CollectTopologyTransaction::run()
{
    debug() << "Collect topology to " << mContractors.front();
    for (const auto &contractorUUID : mContractors) {
        if (contractorUUID == currentNodeUUID()) {
            warning() << "Attempt to initialise operation against itself was prevented. Canceled.";
            return resultDone();
        }
    }
    // Check if Node does not have outgoing FlowAmount;
    if(mTrustLinesManager->firstLevelNeighborsWithOutgoingFlow().size() == 0){
        return resultDone();
    }
    if (!mMaxFlowCalculationCacheManager->isInitiatorCached()) {
        for (auto const &nodeUUIDAndOutgoingFlow : mTrustLinesManager->outgoingFlows()) {
            auto trustLineAmountShared = nodeUUIDAndOutgoingFlow.second;
            mMaxFlowCalculationTrustLineManager->addTrustLine(
                make_shared<MaxFlowCalculationTrustLine>(
                    mNodeUUID,
                    nodeUUIDAndOutgoingFlow.first,
                    trustLineAmountShared));
        }
        sendMessagesOnFirstLevel();
        mMaxFlowCalculationCacheManager->setInitiatorCache();
    }
    sendMessagesToContractors();
    return resultDone();
}

void CollectTopologyTransaction::sendMessagesToContractors()
{
    for (const auto &contractorUUID : mContractors)
        sendMessage<InitiateMaxFlowCalculationMessage>(
            contractorUUID,
            currentNodeUUID());
}

void CollectTopologyTransaction::sendMessagesOnFirstLevel()
{
    vector<NodeUUID> outgoingFlowUuids = mTrustLinesManager->firstLevelNeighborsWithOutgoingFlow();
    for (auto const &nodeUUIDOutgoingFlow : outgoingFlowUuids) {
        sendMessage<MaxFlowCalculationSourceFstLevelMessage>(
            nodeUUIDOutgoingFlow,
            mNodeUUID);
    }
}

const string CollectTopologyTransaction::logHeader() const
{
    stringstream s;
    s << "[CollectTopologyTA: " << currentTransactionUUID() << "]";
    return s.str();
}
