#include "RoutingTablesMessage.h"

RoutingTablesMessage::RoutingTablesMessage() {}

RoutingTablesMessage::RoutingTablesMessage(
    const NodeUUID &senderUUID) :

    SenderMessage(senderUUID) {}

const bool RoutingTablesMessage::isRoutingTableMessage() const {

    return true;
}

const RoutingTablesMessage::PropagationStep RoutingTablesMessage::propagationStep() const {

    return mPropagationStep;
}

const map<const NodeUUID, vector<pair<const NodeUUID, const TrustLineDirection>>>& RoutingTablesMessage::records() const {

    return mRecords;
}