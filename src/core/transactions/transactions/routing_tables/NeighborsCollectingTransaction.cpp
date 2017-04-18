#include "NeighborsCollectingTransaction.h"


/**
 * @param destinationNode - UUID of the node which must be asked for information about it's neighbors.
 * @param hopDistance - determines how many intermediate nodes are present between the node
 *      who created or updated it's trust line and current node.
 *      It helps to decide how many levels of the destination node neighbors
 *      must be discovered in the network.
 */
NeighborsCollectingTransaction::NeighborsCollectingTransaction (
    const NodeUUID &destinationNode,
    const uint8_t hopDistance,
    RoutingTablesHandler *routingTables,
    Logger *logger)
    noexcept :

    BaseTransaction(
        BaseTransaction::RoutingTables_NeighborsCollecting,
        logger),
    mDestinationNodeUUID(destinationNode),
    mHopDistance(hopDistance),
    mRoutingTables(routingTables),
    mSecondLevelNeighborsMustAlsoBeScanned(true)
{}

TransactionResult::SharedConst NeighborsCollectingTransaction::run ()
    noexcept
{
    switch (mStep) {
        case Stages::NeighborsRequestSending:
            return processNeighborsRequestSending();

        case Stages::NeighborsInfoProcessing:
            return processReceivedNeighborsInfo();

        default:
            throw RuntimeError(
                "NeighborsCollectingTransaction::run: "
                    "unexpected stage occurred.");
    }
}

TransactionResult::SharedConst NeighborsCollectingTransaction::processNeighborsRequestSending ()
    noexcept
{
    // ToDo: ensure request was delivered to the remote node. Retry in case of error.

    // Nothing critical would happen in case if request would be lost in the network.
    // Ewen if it would be lost - the worst is that current node would not get complete neighbors info,
    // but it would be collected on the next automatically launched routing tables synchronisation operation.

    // ToDo: ensure routing tables are re-synchronised automatically on regular basis (2-3 times per week?).
    // ToDo: (optimisation) it is important to launch synchronisation in different times, it would increase responses count.
    // (if synchronisation would be launched in the same time always - there is a non-zero probability,
    // that some remote nodes would never be processed, because them would always offline in that time
    // (timezones problem)).

    // ToDo: ask for neighbors checksum first, and run full synchronisation only in case it is different from the checksum calculated on this node.
    // This will dramatically decrease traffic and takes advantage of recursive nature of this operation.


    sendMessage<NeighborsRequestMessage>(
        mDestinationNodeUUID,
        currentNodeUUID(),
        currentTransactionUUID());

    mStep = Stages::NeighborsInfoProcessing;

    // ToDo: use another result type (resultCollectMessagesNoLongerThanTimeout)
    return resultWaitForMessageTypes(
        {Message::RoutingTables_NeighborsResponse}, 3000);
}

TransactionResult::SharedConst NeighborsCollectingTransaction::processReceivedNeighborsInfo ()
    noexcept
{
    if (mContext.empty()) {
        debug() << "No neighbors info message was received. Can't proceed.";
        return resultDone();
    }

    for (size_t i=0; i<mContext.size(); ++i) {
        const auto kMessage = popNextMessage<NeighborsResponseMessage>();
        processNeighbors(
            kMessage->neighbors());
    }
}

void NeighborsCollectingTransaction::processNeighbors (
    const vector<NodeUUID> &neighbors)
    noexcept
{
    if (mHopDistance == 0) {
        if (mSecondLevelNeighborsMustAlsoBeScanned) {
            // Note: all records are written to the database before child transactions would be spawned.
            // This prevents partially written data occurring.
            populateSecondLevelRoutingTable(
                neighbors,
                mDestinationNodeUUID);

            return spawnChildNeighborsScanningTransactions(neighbors);
        }

        return populateThirdLevelRoutingTable(
            neighbors,
            mDestinationNodeUUID);
    }

    else if (mHopDistance == 1)
        return populateThirdLevelRoutingTable(
            neighbors,
            mDestinationNodeUUID);

    throw RuntimeError(
        "NeighborsCollectingTransaction::processNeighbors: "
            "invalid hop distance occurred.");
}

/**
 * Atomically writes (@source -> ∀ @neighbors) into second level routing table.
 */
void NeighborsCollectingTransaction::populateSecondLevelRoutingTable (
    const vector<NodeUUID> &neighbors,
    const NodeUUID &source)
    noexcept
{
    for (const auto kNeighbor : neighbors)
        try {
            mRoutingTables->saveRecordToRT2(
                source,
                kNeighbor);

            debug() << "Record (" << source << " - " << kNeighbor << ") has been written to the RT2.";

        } catch (IOError &) {
            error() << "Record (" << source << " - " << kNeighbor << ") can't be written to the RT2.";
        }

    mRoutingTables->commit();
}

/**
 * Atomically writes (@source -> ∀ @neighbors) into third level routing table.
 */
void NeighborsCollectingTransaction::populateThirdLevelRoutingTable (
    const vector<NodeUUID> &neighbors,
    const NodeUUID &source)
    noexcept
{
    for (const auto kNeighbor : neighbors)
        try {
            mRoutingTables->saveRecordToRT3(
                source,
                kNeighbor);

            debug() << "Record (" << source << " - " << kNeighbor << ") has been written to the RT3.";

        } catch (IOError &) {
            error() << "Record (" << source << " - " << kNeighbor << ") can't be written to the RT3.";
        }

    mRoutingTables->commit();
}

void NeighborsCollectingTransaction::spawnChildNeighborsScanningTransactions (
    const vector<NodeUUID> &neighbors)
    noexcept
{
    for (const auto kNeighbor : neighbors){
        const auto kTransaction = make_shared<NeighborsCollectingTransaction>(
            kNeighbor,
            mHopDistance,
            mRoutingTables,
            mLog);

        // Prevent infinite network scanning
        kTransaction->mSecondLevelNeighborsMustAlsoBeScanned = false;

        launchSubsidiaryTransaction(kTransaction);
        debug() << "Child neighbors scanning transaction spawned with (" << kNeighbor << ") as destination.";
    }
}
