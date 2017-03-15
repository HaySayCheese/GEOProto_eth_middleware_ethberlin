#include "CloseTrustLineTransaction.h"

CloseTrustLineTransaction::CloseTrustLineTransaction(
    const NodeUUID &nodeUUID,
    CloseTrustLineCommand::Shared command,
    TrustLinesManager *manager,
    OperationsHistoryStorage *historyStorage) :

    TrustLineTransaction(
        BaseTransaction::TransactionType::CloseTrustLineTransactionType,
        nodeUUID),
    mCommand(command),
    mTrustLinesManager(manager),
    mOperationsHistoryStorage(historyStorage) {}

CloseTrustLineTransaction::CloseTrustLineTransaction(
    BytesShared buffer,
    TrustLinesManager *manager,
    OperationsHistoryStorage *historyStorage) :

    TrustLineTransaction(
        BaseTransaction::TransactionType::CloseTrustLineTransactionType),
    mTrustLinesManager(manager),
    mOperationsHistoryStorage(historyStorage) {

    deserializeFromBytes(
        buffer);
}

CloseTrustLineCommand::Shared CloseTrustLineTransaction::command() const {

    return mCommand;
}

pair<BytesShared, size_t> CloseTrustLineTransaction::serializeToBytes() const{

    auto parentBytesAndCount = TrustLineTransaction::serializeToBytes();
    auto commandBytesAndCount = mCommand->serializeToBytes();

    size_t bytesCount = parentBytesAndCount.second
                        + commandBytesAndCount.second;

    BytesShared dataBytesShared = tryCalloc(
        bytesCount);
    //-----------------------------------------------------
    memcpy(
        dataBytesShared.get(),
        parentBytesAndCount.first.get(),
        parentBytesAndCount.second);
    //-----------------------------------------------------
    memcpy(
        dataBytesShared.get() + parentBytesAndCount.second,
        commandBytesAndCount.first.get(),
        commandBytesAndCount.second);
    //-----------------------------------------------------
    return make_pair(
        dataBytesShared,
        bytesCount);
}

void CloseTrustLineTransaction::deserializeFromBytes(
    BytesShared buffer) {

    TrustLineTransaction::deserializeFromBytes(
        buffer);

    BytesShared commandBufferShared = tryMalloc(
        CloseTrustLineCommand::kRequestedBufferSize());
    //-----------------------------------------------------
    memcpy(
        commandBufferShared.get(),
        buffer.get() + TrustLineTransaction::kOffsetToDataBytes(),
        CloseTrustLineCommand::kRequestedBufferSize());
    //-----------------------------------------------------
    mCommand = make_shared<CloseTrustLineCommand>(
        commandBufferShared);

}

TransactionResult::SharedConst CloseTrustLineTransaction::run() {

    try {
        switch (mStep) {

            case Stages::CheckUnicity: {
                if (!isTransactionToContractorUnique()) {
                    return conflictErrorResult();
                }

                mStep = Stages::CheckOutgoingDirection;
            }

            case Stages::CheckOutgoingDirection: {
                if (!isOutgoingTrustLineDirectionExisting()) {
                    return trustLineAbsentResult();
                }

                mStep = Stages::CheckDebt;
            }

            case Stages::CheckDebt: {
                if (checkDebt()) {
                    suspendTrustLineDirectionToContractor();

                } else {
                    closeTrustLine();
                    logClosingTrustLineOperation();
                }

                mStep = Stages::CheckContext;
            }

        case Stages::CheckContext: {
            if (!mContext.empty()) {
                return checkTransactionContext();

                } else {

                    if (mRequestCounter < kMaxRequestsCount) {
                        sendMessageToRemoteNode();
                        increaseRequestsCounter();

                    } else {
                        return noResponseResult();
                    }

                }

                return waitingForResponseState();
            }

            default: {
                throw ConflictError("CloseTrustLineTransaction::run: "
                                        "Illegal step execution.");
            }

        }

    } catch (exception &e) {
        throw RuntimeError("CloseTrustLineTransaction::run: "
                               "TransactionUUID -> " + mTransactionUUID.stringUUID() + ". " +
                               "Crashed at step -> " + to_string(mStep) + ". "
                               "Message -> " + e.what());
    }
}

bool CloseTrustLineTransaction::isTransactionToContractorUnique() {

    return true;
}

bool CloseTrustLineTransaction::isOutgoingTrustLineDirectionExisting() {

    return mTrustLinesManager->checkDirection(mCommand->contractorUUID(), TrustLineDirection::Outgoing) ||
           mTrustLinesManager->checkDirection(mCommand->contractorUUID(), TrustLineDirection::Both);
}

bool CloseTrustLineTransaction::checkDebt() {

    return mTrustLinesManager->balanceRange(mCommand->contractorUUID()) == BalanceRange::Positive;
}

void CloseTrustLineTransaction::suspendTrustLineDirectionToContractor() {

    mTrustLinesManager->suspendDirection(
        mCommand->contractorUUID(),
        TrustLineDirection::Outgoing);
}

void CloseTrustLineTransaction::closeTrustLine() {

    mTrustLinesManager->close(
        mCommand->contractorUUID());
}

void CloseTrustLineTransaction::logClosingTrustLineOperation() {

    Record::Shared record = make_shared<TrustLineRecord>(
        uuid(mTransactionUUID),
        TrustLineRecord::TrustLineOperationType::Closing,
        mCommand->contractorUUID());

    mOperationsHistoryStorage->addRecord(
        record);
}

TransactionResult::SharedConst CloseTrustLineTransaction::checkTransactionContext() {

    if (mExpectationResponsesCount == mContext.size()) {
        auto responseMessage = *mContext.begin();

        if (responseMessage->typeID() == Message::MessageTypeID::ResponseMessageType) {
            Response::Shared response = static_pointer_cast<Response>(
                responseMessage);

            switch (response->code()) {

                case RejectTrustLineMessage::kResultCodeRejected: {
                    return resultOk();
                }

                case RejectTrustLineMessage::kResultCodeRejectDelayed: {
                    return resultOk();
                }

                case RejectTrustLineMessage::kResultCodeTransactionConflict: {
                    return transactionConflictResult();
                }

                default: {
                    return unexpectedErrorResult();
                }
            }
        }

        return unexpectedErrorResult();

    } else {
        throw ConflictError("OpenTrustLineTransaction::checkTransactionContext: "
                                "Unexpected context size.");
    }
}

void CloseTrustLineTransaction::sendMessageToRemoteNode() {

    sendMessage<CloseTrustLineMessage>(
        mCommand->contractorUUID(),
        mNodeUUID,
        mTransactionUUID,
        mNodeUUID);
}

TransactionResult::SharedConst CloseTrustLineTransaction::waitingForResponseState() {

    TransactionState *transactionState = new TransactionState(
        microsecondsSinceGEOEpoch(
            utc_now() + pt::microseconds(kConnectionTimeout * 1000)),
        Message::MessageTypeID::ResponseMessageType,
        false);


    return transactionResultFromState(
        TransactionState::SharedConst(transactionState));
}

TransactionResult::SharedConst CloseTrustLineTransaction::resultOk() {

    return transactionResultFromCommand(
        mCommand->resultOk());
}

TransactionResult::SharedConst CloseTrustLineTransaction::trustLineAbsentResult() {

    return transactionResultFromCommand(
        mCommand->trustLineIsAbsentResult());
}

TransactionResult::SharedConst CloseTrustLineTransaction::conflictErrorResult() {

    return transactionResultFromCommand(
        mCommand->resultConflict());
}

TransactionResult::SharedConst CloseTrustLineTransaction::noResponseResult() {

    return transactionResultFromCommand(
        mCommand->resultNoResponse());
}

TransactionResult::SharedConst CloseTrustLineTransaction::transactionConflictResult() {

    return transactionResultFromCommand(
        mCommand->resultTransactionConflict());
}

TransactionResult::SharedConst CloseTrustLineTransaction::unexpectedErrorResult() {

    return transactionResultFromCommand(
        mCommand->unexpectedErrorResult());
}