#ifndef GEO_NETWORK_CLIENT_TRANSACTIONSMANAGER_H
#define GEO_NETWORK_CLIENT_TRANSACTIONSMANAGER_H

#include "../../common/Types.h"
#include "../../common/memory/MemoryUtils.h"

#include "../../common/NodeUUID.h"
#include "../../trust_lines/manager/TrustLinesManager.h"
#include "../../interface/results_interface/interface/ResultsInterface.h"
#include "../../logger/Logger.h"

#include "../../db/uuid_map_block_storage/UUIDMapBlockStorage.h"
#include "../scheduler/TransactionsScheduler.h"

#include "../../interface/commands_interface/commands/BaseUserCommand.h"
#include "../../interface/commands_interface/commands/trust_lines/OpenTrustLineCommand.h"
#include "../../interface/commands_interface/commands/trust_lines/CloseTrustLineCommand.h"
#include "../../interface/commands_interface/commands/trust_lines/SetTrustLineCommand.h"
#include "../../interface/commands_interface/commands/payments/CreditUsageCommand.h"
#include "../../interface/commands_interface/commands/max_flow_calculation/InitiateMaxFlowCalculationCommand.h"

#include "../../network/messages/Message.hpp"
#include "../../network/messages/incoming/trust_lines/AcceptTrustLineMessage.h"
#include "../../network/messages/incoming/trust_lines/RejectTrustLineMessage.h"
#include "../../network/messages/incoming/trust_lines/UpdateTrustLineMessage.h"
#include "../../network/messages/response/Response.h"
#include "../../network/messages/incoming/max_flow_calculation/ReceiveMaxFlowCalculationOnTargetMessage.h"

#include "../transactions/base/BaseTransaction.h"
#include "../transactions/base/UniqueTransaction.h"
#include "../transactions/unique/trust_lines/OpenTrustLineTransaction.h"
#include "../transactions/unique/trust_lines/AcceptTrustLineTransaction.h"
#include "../transactions/unique/trust_lines/CloseTrustLineTransaction.h"
#include "../transactions/unique/trust_lines/RejectTrustLineTransaction.h"
#include "../transactions/unique/trust_lines/SetTrustLineTransaction.h"
#include "../transactions/unique/trust_lines/UpdateTrustLineTransaction.h"
#include "../transactions/regular/payments/CoordinatorPaymentTransaction.h"
#include "../transactions/regular/payments/ReceiverPaymentTransaction.h"
#include "../transactions/max_flow_calculation/InitiateMaxFlowCalculationTransaction.h"
#include "../transactions/max_flow_calculation/ReceiveMaxFlowCalculationOnTargetTransaction.h"
#include "../transactions/max_flow_calculation/ReceiveResultMaxFlowCalculationFromTargetTransaction.h"
#include "../transactions/max_flow_calculation/MaxFlowCalculationSourceFstLevelTransaction.h"
#include "../transactions/max_flow_calculation/MaxFlowCalculationTargetFstLevelTransaction.h"
#include "../transactions/max_flow_calculation/MaxFlowCalculationSourceSndLevelTransaction.h"
#include "../transactions/max_flow_calculation/MaxFlowCalculationTargetSndLevelTransaction.h"
#include "../transactions/max_flow_calculation/ReceiveResultMaxFlowCalculationFromSourceTransaction.h"

#include <boost/signals2.hpp>

#include <string>

using namespace std;
namespace storage = db::uuid_map_block_storage;
namespace signals = boost::signals2;

class TransactionsManager {
    // todo: hsc: tests?
public:
    signals::signal<void(Message::Shared, const NodeUUID&)> transactionOutgoingMessageReadySignal;

public:
    TransactionsManager(
        NodeUUID &nodeUUID,
        as::io_service &IOService,
        TrustLinesManager *trustLinesManager,
        ResultsInterface *resultsInterface,
        Logger *logger);

    void processCommand(
        BaseUserCommand::Shared command);

    void processMessage(
        Message::Shared message);

    // Invokes from Core
    void launchRoutingTableExchangeTransaction(
        const NodeUUID &contractorUUID,
        const TrustLineDirection direction);

private:
    void loadTransactions();

    void launchOpenTrustLineTransaction(
        OpenTrustLineCommand::Shared command);

    void launchSetTrustLineTransaction(
        SetTrustLineCommand::Shared command);

    void launchCloseTrustLineTransaction(
        CloseTrustLineCommand::Shared command);

    void launchInitiateMaxFlowCalculatingTransaction(
        InitiateMaxFlowCalculationCommand::Shared command);

    void launchAcceptTrustLineTransaction(
        AcceptTrustLineMessage::Shared message);

    void launchUpdateTrustLineTransaction(
        UpdateTrustLineMessage::Shared message);

    void launchRejectTrustLineTransaction(
        RejectTrustLineMessage::Shared message);

    void launchReceiveMaxFlowCalculationTransaction(
        ReceiveMaxFlowCalculationOnTargetMessage::Shared message);

    void launchReceiveResultMaxFlowCalculationFromTargetTransaction(
        ResultMaxFlowCalculationFromTargetMessage::Shared message);

    void launchReceiveResultMaxFlowCalculationFromSourceTransaction(
        ResultMaxFlowCalculationFromSourceMessage::Shared message);

    void launchMaxFlowCalculationSourceFstLevelTransaction(
        MaxFlowCalculationSourceFstLevelInMessage::Shared message);

    void launchMaxFlowCalculationTargetFstLevelTransaction(
        MaxFlowCalculationTargetFstLevelInMessage::Shared message);

    void launchMaxFlowCalculationSourceSndLevelTransaction(
        MaxFlowCalculationSourceSndLevelInMessage::Shared message);

    void launchMaxFlowCalculationTargetSndLevelTransaction(
        MaxFlowCalculationTargetSndLevelInMessage::Shared message);

private:
    // Payment transactions
    void launchCoordinatorPaymentTransaction(
        CreditUsageCommand::Shared command);

    void launchReceiverPaymentTransaction(
        ReceiverInitPaymentMessage::Shared message);

private:
    void subscribeForOutgoingMessages(
        BaseTransaction::SendMessageSignal &signal);

    void subscribeForCommandResult(
        TransactionsScheduler::CommandResultSignal &signal);

    void onTransactionOutgoingMessageReady(
        Message::Shared message,
        const NodeUUID &contractorUUID);

    void onCommandResultReady(
        CommandResult::SharedConst result);

private:
    NodeUUID &mNodeUUID;
    as::io_service &mIOService;
    TrustLinesManager *mTrustLines;
    ResultsInterface *mResultsInterface;
    Logger *mLog;

    unique_ptr<storage::UUIDMapBlockStorage> mStorage;
    unique_ptr<TransactionsScheduler> mScheduler;
};

#endif //GEO_NETWORK_CLIENT_TRANSACTIONSMANAGER_H
