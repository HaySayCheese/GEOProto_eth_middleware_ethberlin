#ifndef GEO_NETWORK_CLIENT_GETFOURNODESNEIGHBORBALANCESTRANSACTION_H
#define GEO_NETWORK_CLIENT_GETFOURNODESNEIGHBORBALANCESTRANSACTION_H

#include "../../base/BaseTransaction.h"
#include "../../../../trust_lines/manager/TrustLinesManager.h"
#include "../../../../io/storage/RoutingTablesHandler.h"
#include "../../../../network/messages/cycles/FourNodes/CyclesFourNodesBalancesRequestMessage.h"
#include "../../../../network/messages/cycles/FourNodes/CyclesFourNodesBalancesResponseMessage.h"
#include <set>

class CyclesFourNodesInitTransaction :
    public BaseTransaction {

public:
    CyclesFourNodesInitTransaction(
        const NodeUUID &nodeUUID,
        const NodeUUID &debtorContractorUUID,
        const NodeUUID &creditorContractorUUID,
        TrustLinesManager *manager,
        RoutingTablesHandler *routingTablesHandler,
        Logger *logger);

    TransactionResult::SharedConst run();

public:
    mutable LaunchCloseCycleSignal closeCycleSignal;

protected:
    enum Stages {
        CollectDataAndSendMessage = 1,
        ParseMessageAndCreateCycles
    };

    TransactionResult::SharedConst runCollectDataAndSendMessageStage();
    TransactionResult::SharedConst runParseMessageAndCreateCyclesStage();

protected:
    set<NodeUUID> commonNeighborsForDebtorAndCreditorNodes();

protected:
    NodeUUID mDebtorContractorUUID;
    NodeUUID mCreditorContractorUUID;
    TrustLinesManager *mTrustLinesManager;
    Logger *mLogger;
    RoutingTablesHandler *mRoutingTablesHandler;
};

#endif //GEO_NETWORK_CLIENT_GETFOURNODESNEIGHBORBALANCESTRANSACTION_H
