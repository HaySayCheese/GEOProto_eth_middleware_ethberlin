#include "CycleCloserInitiatorTransaction.h"

CycleCloserInitiatorTransaction::CycleCloserInitiatorTransaction(
    const NodeUUID &kCurrentNodeUUID,
    Path::ConstShared path,
    TrustLinesManager *trustLines,
    StorageHandler *storageHandler,
    Logger *log)
noexcept :

    BasePaymentTransaction(
        BaseTransaction::CycleCloserInitiatorTransaction,
        kCurrentNodeUUID,
        trustLines,
        storageHandler,
        log)
{
    mStep = Stages::Coordinator_AmountReservation;
    mPathStats = make_unique<PathStats>(path);
}

CycleCloserInitiatorTransaction::CycleCloserInitiatorTransaction(
    BytesShared buffer,
    TrustLinesManager *trustLines,
    StorageHandler *storageHandler,
    Logger *log)
throw (bad_alloc) :

    BasePaymentTransaction(
        BaseTransaction::CycleCloserInitiatorTransaction,
        buffer,
        trustLines,
        storageHandler,
        log)
{}


TransactionResult::SharedConst CycleCloserInitiatorTransaction::run()
    throw (RuntimeError, bad_alloc)
{
    info() << "run\tstep: " << mStep;
    switch (mStep) {
        case Stages::Coordinator_AmountReservation:
            return runAmountReservationStage();

        case Stages::Coordinator_PreviousNeighborRequestProcessing:
            return runPreviousNeighborRequestProcessingStage();

        case Stages::Coordinator_FinalPathsConfigurationApproving:
            return runFinalParticipantsRequestsProcessingStage();

        case Stages::Common_VotesChecking:
            return runVotesCheckingStage();

        default:
            throw RuntimeError(
                "CycleCloserInitiatorTransaction::run(): "
                    "invalid transaction step.");
    }
}

TransactionResult::SharedConst CycleCloserInitiatorTransaction::runAmountReservationStage ()
{
    // Check if total outgoing possibilities of this node are not smaller,
    // than total operation amount. In case if so - there is no reason to begin the operation:
    // current node would not be able to pay such an amount.
    // TODO : check if incoming and outgoing balances are not zero and calc minimum of them for init amount
    /*const auto kTotalOutgoingPossibilities = *(mTrustLines->totalOutgoingAmount());
    if (kTotalOutgoingPossibilities < mCommand->amount())
        return resultInsufficientFundsError();*/

    const auto kPathStats = mPathStats.get();
    if (kPathStats->isReadyToSendNextReservationRequest())
        return tryReserveNextIntermediateNodeAmount(kPathStats);

    else if (kPathStats->isWaitingForNeighborReservationResponse())
        return processNeighborAmountReservationResponse();

    else if (kPathStats->isWaitingForNeighborReservationPropagationResponse())
        return processNeighborFurtherReservationResponse();

    else if (kPathStats->isWaitingForReservationResponse())
        return processRemoteNodeResponse();

    throw RuntimeError(
        "CycleCloserInitiatorTransaction::processAmountReservationStage: "
            "unexpected behaviour occured.");

}

TransactionResult::SharedConst CycleCloserInitiatorTransaction::runFinalParticipantsRequestsProcessingStage ()
{
    if (! contextIsValid(Message::Payments_ParticipantsPathsConfigurationRequest))
        // Coordinator already signed the transaction and can't reject it.
        // But the remote intermediate node will newer receive
        // the response and must not sign the transaction.
        return recover("No final configuration request was received. Recovering.");


    const auto kMessage = popNextMessage<ParticipantsConfigurationRequestMessage>();
    info() << "Final payment paths configuration request received from (" << kMessage->senderUUID << ")";

    // Intermediate node requested final payment configuration.
    auto responseMessage = make_shared<ParticipantsConfigurationMessage>(
        currentNodeUUID(),
        currentTransactionUUID(),
        ParticipantsConfigurationMessage::ForIntermediateNode);

    const auto kPathStats = mPathStats.get();
    const auto kPath = kPathStats->path();

    // TODO maby remove
    // If path was dropped (processed, but rejected) - exclude it.
    if (!kPathStats->isValid())
        return resultDone();

    auto kIntermediateNodePathPos = 1;
    while (kIntermediateNodePathPos != kPath->length()) {
        if (kPath->nodes[kIntermediateNodePathPos] == kMessage->senderUUID)
            break;

        kIntermediateNodePathPos++;
    }

    const auto kIncomingNode = kPath->nodes[kIntermediateNodePathPos - 1];
    const auto kOutgoingNode = kPath->nodes[kIntermediateNodePathPos + 1];

    responseMessage->addPath(
        kPathStats->maxFlow(),
        kIncomingNode,
        kOutgoingNode);

#ifdef DEBUG
    debug() << "Added path: ("
            << kIncomingNode << "), ("
            << kOutgoingNode << ") ["
            << kPathStats->maxFlow() << "]";
#endif

    const auto kReceiverNodeUUID = kMessage->senderUUID;
    sendMessage(
        kReceiverNodeUUID,
        responseMessage);

#ifdef DEBUG
    debug() << "Final payment path configuration message sent to the (" << kReceiverNodeUUID << ")";
#endif

    mNodesRequestedFinalConfiguration.insert(kMessage->senderUUID);
    if (mNodesRequestedFinalConfiguration.size() == mParticipantsVotesMessage->participantsCount()){

#ifdef DEBUG
        debug() << "All involved nodes has been requested final payment path configuration. "
            "Begin waiting for the signed votes message";
#endif

        mStep = Stages::Common_VotesChecking;
        return resultWaitForMessageTypes(
            {Message::Payments_ParticipantsVotes},
            maxNetworkDelay(2));
    }

    // Waiting for the rest nodes to request final payment paths configuration
    return resultWaitForMessageTypes(
        {Message::Payments_ParticipantsPathsConfigurationRequest},
        maxNetworkDelay(1));
}


TransactionResult::SharedConst CycleCloserInitiatorTransaction::runVotesCheckingStage ()
{
    // Votes message may be received twice:
    // First time - as a request to check the transaction and to sing it in case if all correct.
    // Second time - as a command to commit/rollback the transaction.
    if (mTransactionIsVoted)
        return runVotesConsistencyCheckingStage();


    if (! contextIsValid(Message::Payments_ParticipantsVotes))
        return reject("No participants votes received. Canceling.");


    const auto kCurrentNodeUUID = currentNodeUUID();
    auto message = popNextMessage<ParticipantsVotesMessage>();
    info() << "Votes message received";


    try {
        // Check if current node is listed in the votes list.
        // This check is needed to prevent processing message in case of missdelivering.
        message->vote(kCurrentNodeUUID);

    } catch (NotFoundError &) {
        // It seems that current node wasn't listed in the votes list.
        // This is possible only in case, when one node takes part in 2 parallel transactions,
        // that have common UUID (transactions UUIDs collision).
        // The probability of this is very small, but is present.
        //
        // In this case - the message must be simply ignored.

        info() << "Votes message ignored due to transactions UUIDs collision detected.";
        info() << "Waiting for another votes message.";

        return resultWaitForMessageTypes(
            {Message::Payments_ParticipantsVotes},
            maxNetworkDelay(message->participantsCount())); // ToDo: kMessage->participantsCount() must not be used (it is invalid)
    }


    if (message->containsRejectVote())
        // Some node rejected the transaction.
        // This node must simply roll back it's part of transaction and exit.
        // No further message propagation is needed.
        reject("Some participant node has been rejected the transaction. Rolling back.");


    // Votes message must be saved for further processing on next awakening.
    mParticipantsVotesMessage = message;


    mStep = Stages::Common_FinalPathsConfigurationChecking;
    return resultWaitForMessageTypes(
        {Message::Payments_ParticipantsPathsConfiguration},
        maxCoordinatorResponseTimeout());
}


/**
 * @brief CycleCloserInitiatorTransaction::propagateVotesList
 * Collects all nodes from all paths into one votes list,
 * and propagates it to the next node in the votes list.
 */
TransactionResult::SharedConst CycleCloserInitiatorTransaction::propagateVotesListAndWaitForConfigurationRequests ()
{
    const auto kCurrentNodeUUID = currentNodeUUID();
    const auto kTransactionUUID = currentTransactionUUID();

    // TODO: additional check if payment is correct

    // Prevent simple transaction rolling back
    // todo: make this atomic
    mTransactionIsVoted = true;

    auto message = make_shared<ParticipantsVotesMessage>(
        kCurrentNodeUUID,
        kTransactionUUID,
        kCurrentNodeUUID);

#ifdef DEBUG
    uint16_t totalParticipantsCount = 0;
#endif

    // If paths wasn't processed - exclude it (all it's nodes).
    // Unprocessed paths may occur, because paths are loaded into the transaction in batch,
    // some of them may be used, and some may be left unprocessed.
    auto kPathStats = mPathStats.get();

    for (const auto &nodeUUID : kPathStats->path()->nodes) {
        // By the protocol, coordinator node must be excluded from the message.
        // Only coordinator may emit ParticipantsApprovingMessage into the network.
        // It is supposed, that in case if it was emitted - than coordinator approved the transaction.
        //
        // TODO: [mvp] [cryptography] despite this, coordinator must sign the message,
        // so the other nodes would be possible to know that this message was emitted by the coordinator.
        if (nodeUUID == kCurrentNodeUUID)
            continue;

        message->addParticipant(nodeUUID);

#ifdef DEBUG
        totalParticipantsCount++;
#endif
    }


#ifdef DEBUG
    debug() << "Total participants included: " << totalParticipantsCount;
    debug() << "Participants order is the next:";
    for (const auto kNodeUUIDAndVote : message->votes()) {
        debug() << kNodeUUIDAndVote.first;
    }
#endif

    // Begin message propagation
    sendMessage(
        message->firstParticipant(),
        message);

    info() << "Votes message constructed and sent to the (" << message->firstParticipant() << ")";
    info() << "Begin accepting final payment paths configuration requests.";


    // Participants votes message would be used further
    // in final paths configurations requests processing stage.
    mParticipantsVotesMessage = message;

    // Now coordinator begins responding to the final configuration requests of the participants.
    mStep = Stages::Coordinator_FinalPathsConfigurationApproving;
    return resultWaitForMessageTypes(
        {Message::Payments_ParticipantsPathsConfigurationRequest},
        maxNetworkDelay(1));
}

TransactionResult::SharedConst CycleCloserInitiatorTransaction::tryReserveNextIntermediateNodeAmount (
    PathStats *pathStats)
{
    /*
     * Nodes scheme:
     *  R - remote node;
     *  S - next node in path after remote one;
     */

    try {
        const auto R_UUIDAndPos = pathStats->nextIntermediateNodeAndPos();
        const auto R_UUID = R_UUIDAndPos.first;
        const auto R_PathPosition = R_UUIDAndPos.second;

        const auto S_PathPosition = R_PathPosition + 1;
        const auto S_UUID = pathStats->path()->nodes[S_PathPosition];

        if (R_PathPosition == 1) {
            if (pathStats->isNeighborAmountReserved())
                return askNeighborToApproveFurtherNodeReservation(
                    R_UUID,
                    pathStats);

            else
                return askNeighborToReserveAmount(
                    R_UUID,
                    pathStats);

        } else {
            info() << "Processing " << int(R_PathPosition) << " node in path: (" << R_UUID << ").";

            return askRemoteNodeToApproveReservation(
                pathStats,
                R_UUID,
                R_PathPosition,
                S_UUID);
        }

    } catch(NotFoundError) {
        info() << "No unprocessed paths are left.";
        info() << "Requested amount can't be collected. Canceling.";
        return resultDone();
    }
}

TransactionResult::SharedConst CycleCloserInitiatorTransaction::askNeighborToReserveAmount(
    const NodeUUID &neighbor,
    PathStats *path)
{
    const auto kCurrentNode = currentNodeUUID();
    const auto kTransactionUUID = currentTransactionUUID();

    if (! mTrustLines->isNeighbor(neighbor)){
        // Internal process error.
        // No next path must be selected.
        // Transaction execution must be cancelled.

        error() << "Invalid path occurred. Node (" << neighbor << ") is not listed in first level contractors list.";
        error() << "This may signal about protocol/data manipulations.";

        throw RuntimeError(
            "CycleCloserInitiatorTransaction::tryReserveNextIntermediateNodeAmount: "
                "invalid first level node occurred. ");
    }

    // Note: copy of shared pointer is required
    const auto kAvailableOutgoingAmount =  mTrustLines->availableOutgoingAmount(neighbor);
    const auto kReservationAmount = *kAvailableOutgoingAmount;

    if (kReservationAmount == 0) {
        info() << "No payment amount is available for (" << neighbor << "). "
            "Switching to another path.";

        return resultDone();
    }

    // Try reserve amount locally.
    path->shortageMaxFlow(kReservationAmount);
    path->setNodeState(
        1,
        PathStats::NeighbourReservationRequestSent);

    reserveOutgoingAmount(
        neighbor,
        kReservationAmount);


    sendMessage<IntermediateNodeReservationRequestMessage>(
        neighbor,
        kCurrentNode,
        kTransactionUUID,
        path->maxFlow());

    return resultWaitForMessageTypes(
        {Message::Payments_IntermediateNodeReservationResponse},
        maxNetworkDelay(1));
}

TransactionResult::SharedConst CycleCloserInitiatorTransaction::askNeighborToApproveFurtherNodeReservation(
    const NodeUUID& neighbor,
    PathStats *path)
{
    const auto kCoordinator = currentNodeUUID();
    const auto kTransactionUUID = currentTransactionUUID();
    const auto kNeighborPathPosition = 1;
    const auto kNextAfterNeighborNode = path->path()->nodes[kNeighborPathPosition+1];

    // Note:
    // no check of "neighbor" node is needed here.
    // It was done on previous step.


    sendMessage<CoordinatorReservationRequestMessage>(
        neighbor,
        kCoordinator,
        kTransactionUUID,
        path->maxFlow(),
        kNextAfterNeighborNode);

    info() << "Further amount reservation request sent to the node (" << neighbor << ") [" << path->maxFlow() << "]";

    path->setNodeState(
        kNeighborPathPosition,
        PathStats::ReservationRequestSent);


    return resultWaitForMessageTypes(
        {Message::Payments_CoordinatorReservationResponse},
        kMaxMessageTransferLagMSec);
}

TransactionResult::SharedConst CycleCloserInitiatorTransaction::processNeighborAmountReservationResponse()
{
    if (! contextIsValid(Message::Payments_IntermediateNodeReservationResponse)) {
        info() << "No neighbor node response received. Switching to another path.";
        return resultDone();
    }


    auto message = popNextMessage<IntermediateNodeReservationResponseMessage>();
    // todo: check message sender

    if (message->state() != IntermediateNodeReservationResponseMessage::Accepted) {
        error() << "Neighbor node doesn't approved reservation request";
        return resultDone();
    }


    info() << "(" << message->senderUUID << ") approved reservation request.";
    auto path = mPathStats.get();
    path->setNodeState(
        1, PathStats::NeighbourReservationApproved);


    return runAmountReservationStage();
}

TransactionResult::SharedConst CycleCloserInitiatorTransaction::processNeighborFurtherReservationResponse()
{
    if (! contextIsValid(Message::Payments_CoordinatorReservationResponse)) {
        info() << "Switching to another path.";
        return resultDone();
    }

    auto message = popNextMessage<CoordinatorReservationResponseMessage>();
    if (message->state() != CoordinatorReservationResponseMessage::Accepted) {
        info() << "Neighbor node doesn't accepted coordinator request.";
        return resultDone();
    }


    auto path = mPathStats.get();
    path->setNodeState(
        1,
        PathStats::ReservationApproved);
    info() << "Neighbor node accepted coordinator request. Reserved: " << message->amountReserved();

    path->shortageMaxFlow(message->amountReserved());
    info() << "Path max flow is now " << path->maxFlow();


    return runAmountReservationStage();
}

TransactionResult::SharedConst CycleCloserInitiatorTransaction::askRemoteNodeToApproveReservation(
    PathStats* path,
    const NodeUUID& remoteNode,
    const byte remoteNodePosition,
    const NodeUUID& nextNodeAfterRemote)
{
    const auto kCoordinator = currentNodeUUID();
    const auto kTransactionUUID = currentTransactionUUID();

    sendMessage<CoordinatorReservationRequestMessage>(
        remoteNode,
        kCoordinator,
        kTransactionUUID,
        path->maxFlow(),
        nextNodeAfterRemote);

    path->setNodeState(
        remoteNodePosition,
        PathStats::ReservationRequestSent);

    if (path->isLastIntermediateNodeProcessed()) {
        info() << "Further amount reservation request sent to the last node (" << remoteNode << ") ["
               << path->maxFlow() << ", next node - (" << nextNodeAfterRemote << ")]";
        mStep = Coordinator_PreviousNeighborRequestProcessing;
        const auto kTimeout = kMaxMessageTransferLagMSec * remoteNodePosition;
        return resultWaitForMessageTypes(
            {Message::Payments_IntermediateNodeReservationRequest},
            kTimeout);
    }
    info() << "Further amount reservation request sent to the node (" << remoteNode << ") ["
           << path->maxFlow() << ", next node - (" << nextNodeAfterRemote << ")]";

    // Response from te remote node will go throught other nodes in the path.
    // So them would be able to shortage it's reservations (if needed).
    // Total wait timeout must take note of this.
    const auto kTimeout = kMaxMessageTransferLagMSec * remoteNodePosition;
    return resultWaitForMessageTypes(
        {Message::Payments_CoordinatorReservationResponse},
        kTimeout);
}

TransactionResult::SharedConst CycleCloserInitiatorTransaction::processRemoteNodeResponse()
{
    if (! contextIsValid(Message::Payments_CoordinatorReservationResponse)){
        error() << "Can't pay.";
        return resultDone();
    }


    const auto message = popNextMessage<CoordinatorReservationResponseMessage>();
    auto path = mPathStats.get();

    /*
     * Nodes scheme:
     * R - remote node;
     */

    const auto R_UUIDAndPos = path->currentIntermediateNodeAndPos();
    const auto R_PathPosition = R_UUIDAndPos.second;


    if (0 == message->amountReserved()) {
        path->setUnusable();
        path->setNodeState(
            R_PathPosition,
            PathStats::ReservationRejected);

        info() << "Remote node rejected reservation. Can't pay";
        return resultDone();

    } else {
        const auto reservedAmount = message->amountReserved();

        path->shortageMaxFlow(reservedAmount);
        path->setNodeState(
            R_PathPosition,
            PathStats::ReservationApproved);

        info() << "(" << message->senderUUID << ") reserved " << reservedAmount;
        info() << "Path max flow is now " << path->maxFlow();

        if (path->isLastIntermediateNodeProcessed()) {
            const auto kTotalAmount = totalReservedByAllPaths();

            info() << "Current path reservation finished";
            info() << "Total collected amount by all paths: " << kTotalAmount;

            return propagateVotesListAndWaitForConfigurationRequests();
        }

        info() << "Go to the next node in path";
        return tryReserveNextIntermediateNodeAmount(path);
    }
}

TransactionResult::SharedConst CycleCloserInitiatorTransaction::runPreviousNeighborRequestProcessingStage()
{
    if (! contextIsValid(Message::Payments_IntermediateNodeReservationRequest))
        return reject("No amount reservation request was received. Rolled back.");


    const auto kMessage = popNextMessage<IntermediateNodeReservationRequestMessage>();
    const auto kNeighbor = kMessage->senderUUID;
    info() << "Coordiantor payment operation from node (" << kNeighbor << ")";
    info() << "Requested amount reservation: " << kMessage->amount();


    // Note: (copy of shared pointer is required)
    const auto kIncomingAmount = mTrustLines->availableIncomingAmount(kNeighbor);
    const auto kReservationAmount =
        min(kMessage->amount(), *kIncomingAmount);

    if (0 == kReservationAmount || ! reserveIncomingAmount(kNeighbor, kReservationAmount)) {
        sendMessage<IntermediateNodeReservationResponseMessage>(
            kNeighbor,
            currentNodeUUID(),
            currentTransactionUUID(),
            ResponseMessage::Rejected);
        return reject("No incoming amount reservation is possible. Rolled back.");
    }

    sendMessage<IntermediateNodeReservationResponseMessage>(
        kNeighbor,
        currentNodeUUID(),
        currentTransactionUUID(),
        ResponseMessage::Accepted);

    mStep = Coordinator_AmountReservation;
    const auto kTimeout = kMaxMessageTransferLagMSec;
    return resultWaitForMessageTypes(
        {Message::Payments_CoordinatorReservationResponse},
        kTimeout);
}

pair<BytesShared, size_t> CycleCloserInitiatorTransaction::serializeToBytes() const
throw (bad_alloc)
{
    // todo: add implementation
    return make_pair(make_shared<byte>(0), 0);
}

void CycleCloserInitiatorTransaction::deserializeFromBytes(BytesShared buffer)
{
    // todo: add implementation
}

TrustLineAmount CycleCloserInitiatorTransaction::totalReservedByAllPaths() const
{
    TrustLineAmount totalAmount = 0;

    auto path = mPathStats.get();

    if (path->isValid())
        totalAmount += path->maxFlow();

    return totalAmount;
}

const string CycleCloserInitiatorTransaction::logHeader() const
{
    stringstream s;
    s << "[CycleCloserInitiatorTA: " << currentTransactionUUID().stringUUID() << "] ";

    return s.str();
}

TransactionResult::SharedConst CycleCloserInitiatorTransaction::approve()
{
    BasePaymentTransaction::approve();
    return resultDone();
}

TransactionResult::SharedConst CycleCloserInitiatorTransaction::recover(
    const char *message)
{
    BasePaymentTransaction::recover(
        message);

    // TODO: implement me correct.
    return resultDone();
}

TransactionResult::SharedConst CycleCloserInitiatorTransaction::reject(
    const char *message)
{
    BasePaymentTransaction::reject(message);

    return resultDone();
}