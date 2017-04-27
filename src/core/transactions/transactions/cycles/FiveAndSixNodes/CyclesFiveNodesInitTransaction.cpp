#include "CyclesFiveNodesInitTransaction.h"

const BaseTransaction::TransactionType CyclesFiveNodesInitTransaction::transactionType() const{
    return BaseTransaction::TransactionType::Cycles_FiveNodesInitTransaction;
}

TransactionResult::SharedConst CyclesFiveNodesInitTransaction::runCollectDataAndSendMessagesStage() {
    vector<NodeUUID> firstLevelNodesNegativeBalance = mTrustLinesManager->firstLevelNeighborsWithNegativeBalance();
    vector<NodeUUID> firstLevelNodesPositiveBalance = mTrustLinesManager->firstLevelNeighborsWithPositiveBalance();
    vector<NodeUUID> path;
    path.push_back(mNodeUUID);
    TrustLineBalance zeroBalance = 0;
    for(const auto &value: firstLevelNodesNegativeBalance)
        sendMessage<CyclesFiveNodesInBetweenMessage>(
            value,
            path
        );
    for(const auto &value: firstLevelNodesPositiveBalance)
            sendMessage<CyclesFiveNodesInBetweenMessage>(
                value,
                path
            );
    mStep = Stages::ParseMessageAndCreateCycles;
    return resultAwaikAfterMilliseconds(mkWaitingForResponseTime);
}

CyclesFiveNodesInitTransaction::CyclesFiveNodesInitTransaction(
    const NodeUUID &nodeUUID,
    TrustLinesManager *manager,
    Logger *logger) :
    CyclesBaseFiveSixNodesInitTransaction(
        BaseTransaction::TransactionType::Cycles_FiveNodesInitTransaction,
        nodeUUID,
        manager,
        logger)
{};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
TransactionResult::SharedConst CyclesFiveNodesInitTransaction::runParseMessageAndCreateCyclesStage() {
    if (mContext.size() == 0) {
        info() << "CyclesFiveNodesInitTransaction::runParseMessageAndCreateCyclesStage: "
                "No responses messages "
                "Can't create cycles;";

        return resultDone();
    }
    TrustLineBalance zeroBalance = 0;
    CycleMap mCreditors;
    TrustLineBalance creditorsStepFlow;
    for(const auto &mess: mContext){
        auto message = static_pointer_cast<CyclesSixNodesBoundaryMessage>(mess);
        const auto stepPath = make_shared<vector<NodeUUID>>(message->Path());
        //  It has to be exactly nodes count in path
        if (stepPath->size() != 2)
            continue;
        creditorsStepFlow = mTrustLinesManager->balance((*stepPath)[1]);
        //  If it is Debtor branch - skip it
        if (creditorsStepFlow > zeroBalance)
            continue;
        //  Check all Boundary Nodes and add it to map if all checks path
        for (auto &NodeUUID: message->BoundaryNodes()){
            //  Prevent loop on cycles path
            if (NodeUUID == stepPath->front())
                continue;
            //  NodeUUIDAndBalance.second - already minimum balance on creditors branch
            //  For not tu use abc for every balance on debtors branch - just change sign of these balance
            mCreditors.insert(make_pair(
                NodeUUID,
                stepPath));
        }
    }
    vector<NodeUUID> stepPathDebtors;
    //    Create Cycles comparing BoundaryMessages data with debtors map
    #ifdef TESTS
        vector<vector<NodeUUID>> ResultCycles;
    #endif
    TrustLineBalance debtorsStepFlow;
    TrustLineBalance commonStepMaxFlow;
    for(const auto &mess: mContext) {
        auto message = static_pointer_cast<CyclesSixNodesBoundaryMessage>(mess);
        debtorsStepFlow = mTrustLinesManager->balance(message->Path()[1]);
        //  If it is Creditors branch - skip it
        if (debtorsStepFlow < zeroBalance)
            continue;
        stepPathDebtors = message->Path();
        //  It has to be exactly nodes count in path
        if (stepPathDebtors.size() != 3)
            continue;
        for (const auto &kNodeUUID: message->BoundaryNodes()) {
            //  Prevent loop on cycles path
            if (kNodeUUID == stepPathDebtors.front())
                continue;

            auto NodeUIIDAndPathRange = mCreditors.equal_range(kNodeUUID);
            for (auto s_it = NodeUIIDAndPathRange.first; s_it != NodeUIIDAndPathRange.second; ++s_it) {
                //  Find minMax flow between 3 value. 1 in map. 1 in boundaryNodes. 1 we get from creditor first node in path
                vector <NodeUUID> stepCyclePath = {stepPathDebtors[0],
                                                   stepPathDebtors[1],
                                                   stepPathDebtors[2],
                                                   kNodeUUID,
                                                   s_it->second->back()};
                auto sCycle = make_shared<vector<NodeUUID>>(stepCyclePath);
                closeCycleSignal(sCycle);
                #ifdef TESTS
                    ResultCycles.push_back(stepCyclePath);
                #endif
                // Todo run cycles
                // Потрібно ставити первірку на доречність перекриття цилів
                // Ця транакція має верта нам дані про те через який трастлайн неможна зробити перерозрахунок
            }
        }
    }
    #ifdef TESTS
    cout << "CyclesSixNodesInitTransaction::ResultCyclesCount " << to_string(ResultCycles.size()) << endl;
    for (vector<NodeUUID> KCyclePath: ResultCycles){
        stringstream ss;
        copy(KCyclePath.begin(), KCyclePath.end(), ostream_iterator<NodeUUID>(ss, ","));
        cout << "CyclesSixNodesInitTransaction::CyclePath " << ss.str() << endl;
    }
    cout << "CyclesSixNodesInitTransaction::End" << endl;
    #endif
    mContext.clear();
    return resultDone();
}
#pragma clang diagnostic pop
