#include "ConfirmationRequiredMessagesQueue.h"


ConfirmationRequiredMessagesQueue::ConfirmationRequiredMessagesQueue(
    const SerializedEquivalent equivalent,
    const NodeUUID &contractorUUID)
    noexcept:
    mEquivalent(equivalent),
    mContractorUUID(contractorUUID)
{
    resetInternalTimeout();
    mNextSendingAttemptDateTime = utc_now() + boost::posix_time::seconds(mNextTimeoutSeconds);
}

bool ConfirmationRequiredMessagesQueue::enqueue(
    TransactionMessage::Shared message)
{
    switch (message->typeID()) {
        case Message::TrustLines_SetIncoming: {
            updateTrustLineNotificationInTheQueue(
                message);
            return true;
        }
        case Message::TrustLines_CloseOutgoing: {
            updateTrustLineCloseNotificationInTheQueue(
                message);
            return true;
        }
        case Message::GatewayNotification: {
            updateGatewayNotificationInTheQueue(
                message);
            return true;
        }
        case Message::TrustLines_SetIncomingFromGateway: {
            updateTrustLineFromGatewayNotificationInTheQueue(
                message);
            return true;
        }
        default:
            return false;
    }
}

bool ConfirmationRequiredMessagesQueue::tryProcessConfirmation(
    ConfirmationMessage::Shared confirmationMessage)
{
    if (mMessages.erase(confirmationMessage->transactionUUID()) > 0) {
        // Remote node responded with the valid response.
        // Internal timeout should be reset.
        resetInternalTimeout();
        return true;
    }

    return false;
}

const DateTime &ConfirmationRequiredMessagesQueue::nextSendingAttemptDateTime()
    noexcept
{
    return mNextSendingAttemptDateTime;
}

const map<TransactionUUID, TransactionMessage::Shared> &ConfirmationRequiredMessagesQueue::messages()
    noexcept
{
    // Exponentially increase re-sending timeout up to 10+ minutes.
    if (mNextTimeoutSeconds < 60 * 10) {
        mNextTimeoutSeconds *= 2;
    }

    mNextSendingAttemptDateTime = utc_now() + boost::posix_time::seconds(mNextTimeoutSeconds);

    return mMessages;
}

const size_t ConfirmationRequiredMessagesQueue::size() const
    noexcept
{
    return mMessages.size();
}

void ConfirmationRequiredMessagesQueue::resetInternalTimeout()
    noexcept
{
    mNextTimeoutSeconds = 4;
}

void ConfirmationRequiredMessagesQueue::updateTrustLineNotificationInTheQueue(
    TransactionMessage::Shared message)
{
    // Only one SetIncomingTrustLineMessage should be in the queue in one moment of time.
    // queue must contains only newest one notification, all other must be removed.
    for (auto it = mMessages.cbegin(); it != mMessages.cend();) {
        const auto kMessage = it->second;

        if (kMessage->typeID() == Message::TrustLines_SetIncoming) {
            mMessages.erase(it++);
            signalRemoveMessageFromStorage(
                mContractorUUID,
                mEquivalent,
                kMessage->typeID());
        } else {
            ++it;
        }
    }

    mMessages[message->transactionUUID()] = message;
    signalSaveMessageToStorage(
        mContractorUUID,
        message);
}

void ConfirmationRequiredMessagesQueue::updateTrustLineFromGatewayNotificationInTheQueue(
    TransactionMessage::Shared message)
{
    // Only one SetIncomingTrustLineFromGatewayMessage should be in the queue in one moment of time.
    // queue must contains only newest one notification, all other must be removed.
    for (auto it = mMessages.cbegin(); it != mMessages.cend();) {
        const auto kMessage = it->second;

        if (kMessage->typeID() == Message::TrustLines_SetIncomingFromGateway) {
            mMessages.erase(it++);
            signalRemoveMessageFromStorage(
                mContractorUUID,
                mEquivalent,
                kMessage->typeID());
        } else {
            ++it;
        }
    }

    mMessages[message->transactionUUID()] = message;
    signalSaveMessageToStorage(
        mContractorUUID,
        message);
}

// todo : discuss if need keep only one message in queue
void ConfirmationRequiredMessagesQueue::updateTrustLineCloseNotificationInTheQueue(
    TransactionMessage::Shared message)
{
    // Only one CloseOutgoingTrustLineMessage should be in the queue in one moment of time.
    // queue must contains only newest one notification, all other must be removed.
    for (auto it = mMessages.cbegin(); it != mMessages.cend();) {
        const auto kMessage = it->second;

        if (kMessage->typeID() == Message::TrustLines_CloseOutgoing) {
            mMessages.erase(it++);
            signalRemoveMessageFromStorage(
                mContractorUUID,
                mEquivalent,
                kMessage->typeID());
        } else {
            ++it;
        }
    }

    mMessages[message->transactionUUID()] = message;
    signalSaveMessageToStorage(
        mContractorUUID,
        message);
}

void ConfirmationRequiredMessagesQueue::updateGatewayNotificationInTheQueue(
    TransactionMessage::Shared message)
{
    // Only one GatewayNotificationMessage should be in the queue in one moment of time.
    // queue must contains only newest one notification, all other must be removed.
    for (auto it = mMessages.cbegin(); it != mMessages.cend();) {
        const auto kMessage = it->second;

        if (kMessage->typeID() == Message::GatewayNotification) {
            mMessages.erase(it++);
            signalRemoveMessageFromStorage(
                mContractorUUID,
                mEquivalent,
                kMessage->typeID());
        } else {
            ++it;
        }
    }

    mMessages[message->transactionUUID()] = message;
    signalSaveMessageToStorage(
        mContractorUUID,
        message);
}
