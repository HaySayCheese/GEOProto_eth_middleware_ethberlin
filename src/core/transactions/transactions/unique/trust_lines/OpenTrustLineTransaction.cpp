#include "OpenTrustLineTransaction.h"

OpenTrustLineTransaction::OpenTrustLineTransaction(
    const NodeUUID &nodeUUID,
    OpenTrustLineCommand::Shared command,
    TrustLinesManager *manager,
    StorageHandler *storageHandler,
    Logger *logger) :

    TrustLineTransaction(
        BaseTransaction::TransactionType::OpenTrustLineTransactionType,
        nodeUUID,
        logger),
    mCommand(command),
    mTrustLinesManager(manager),
    mStorageHandler(storageHandler) {}

OpenTrustLineTransaction::OpenTrustLineTransaction(
    BytesShared buffer,
    TrustLinesManager *manager,
    StorageHandler *storageHandler,
    Logger *logger) :

    TrustLineTransaction(
        BaseTransaction::TransactionType::OpenTrustLineTransactionType,
        logger),
    mTrustLinesManager(manager),
    mStorageHandler(storageHandler) {

    deserializeFromBytes(
        buffer);
}

OpenTrustLineCommand::Shared OpenTrustLineTransaction::command() const {

    return mCommand;
}

pair<BytesShared, size_t> OpenTrustLineTransaction::serializeToBytes() const {

    auto parentBytesAndCount = TrustLineTransaction::serializeToBytes();
    auto commandBytesAndCount = mCommand->serializeToBytes();

    size_t bytesCount = parentBytesAndCount.second
                        + commandBytesAndCount.second;

    BytesShared dataBytesShared = tryMalloc(
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

void OpenTrustLineTransaction::deserializeFromBytes(
    BytesShared buffer) {

    TrustLineTransaction::deserializeFromBytes(
        buffer);

    BytesShared commandBufferShared = tryMalloc(
        OpenTrustLineCommand::kRequestedBufferSize());
    //-----------------------------------------------------
    memcpy(
        commandBufferShared.get(),
        buffer.get() + TrustLineTransaction::kOffsetToDataBytes(),
        OpenTrustLineCommand::kRequestedBufferSize());
    //-----------------------------------------------------
    mCommand = make_shared<OpenTrustLineCommand>(
        commandBufferShared);
}

TransactionResult::SharedConst OpenTrustLineTransaction::run() {

    try {
        switch (mStep) {

            case Stages::CheckUnicity: {
                if (!isTransactionToContractorUnique()) {
                    return resultConflictWithOtherOperation();
                }

                mStep = Stages::CheckOutgoingDirection;
            }

            case Stages::CheckOutgoingDirection: {
                if (isOutgoingTrustLineDirectionExisting()) {
                    return resultTrustLineIsAlreadyPresent();
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
                        return resultRemoteNodeIsInaccessible();
                    }

                }
                return waitingForResponseState();
            }

            default: {
                throw ConflictError("OpenTrustLineTransaction::run: "
                                        "Illegal step execution.");
            }

        }

    } catch (exception &e){
        throw RuntimeError("OpenTrustLineTransaction::run: "
                               "TransactionUUID -> " + mTransactionUUID.stringUUID() + ". " +
                               "Crashed at step -> " + to_string(mStep) );
    }
}

bool OpenTrustLineTransaction::isTransactionToContractorUnique() {

    return true;
}

bool OpenTrustLineTransaction::isOutgoingTrustLineDirectionExisting() {

    return mTrustLinesManager->checkDirection(mCommand->contractorUUID(), TrustLineDirection::Outgoing) ||
        mTrustLinesManager->checkDirection(mCommand->contractorUUID(), TrustLineDirection::Both);
}

TransactionResult::SharedConst OpenTrustLineTransaction::checkTransactionContext() {

    if (mkExpectationResponsesCount == mContext.size()) {
        auto responseMessage = *mContext.begin();

        if (responseMessage->typeID() == Message::MessageType::ResponseMessageType) {
            Response::Shared response = static_pointer_cast<Response>(
                responseMessage);

            switch (response->code()) {

                case AcceptTrustLineMessage::kResultCodeAccepted: {
                    openTrustLine();
                    logOpeningTrustLineOperation();

                    if (!mTrustLinesManager->checkDirection(
                        mCommand->contractorUUID(),
                        TrustLineDirection::Both)) {
                        const auto kTransaction = make_shared<TrustLineStatesHandlerTransaction>(
                            currentNodeUUID(),
                            currentNodeUUID(),
                            currentNodeUUID(),
                            mCommand->contractorUUID(),
                            TrustLineStatesHandlerTransaction::TrustLineState::Created,
                            0,
                            mTrustLinesManager,
                            mStorageHandler,
                            mLog);
                        launchSubsidiaryTransaction(kTransaction);
                    }

                    return resultOk();
                }

                case AcceptTrustLineMessage::kResultCodeConflict: {
                    return resultConflictWithOtherOperation();
                }

                default:{
                    break;
                }

            }

        }
        return resultProtocolError();

    } else {
        throw ConflictError("OpenTrustLineTransaction::checkTransactionContext: "
                                "Unexpected context size.");
    }
}

void OpenTrustLineTransaction::sendMessageToRemoteNode() {

    sendMessage<OpenTrustLineMessage>(
        mCommand->contractorUUID(),
        mNodeUUID,
        mTransactionUUID,
        mCommand->amount());
}

TransactionResult::SharedConst OpenTrustLineTransaction::waitingForResponseState() {

    TransactionState *transactionState = new TransactionState(
        microsecondsSinceGEOEpoch(
            utc_now() + pt::microseconds(kConnectionTimeout * 1000)),
        Message::MessageType::ResponseMessageType,
        false);

    return transactionResultFromState(
        TransactionState::SharedConst(
            transactionState));
}

void OpenTrustLineTransaction::openTrustLine() {

    mTrustLinesManager->open(
        mCommand->contractorUUID(),
        mCommand->amount());

}

void OpenTrustLineTransaction::logOpeningTrustLineOperation() {

    TrustLineRecord::Shared record = make_shared<TrustLineRecord>(
        uuid(mTransactionUUID),
        TrustLineRecord::TrustLineOperationType::Opening,
        mCommand->contractorUUID(),
        mCommand->amount());

    auto ioTransaction = mStorageHandler->beginTransaction();
    ioTransaction->historyStorage()->saveTrustLineRecord(record);
}

TransactionResult::SharedConst OpenTrustLineTransaction::resultOk() {

    return transactionResultFromCommand(
            mCommand->responseCreated());
}

TransactionResult::SharedConst OpenTrustLineTransaction::resultTrustLineIsAlreadyPresent() {

    return transactionResultFromCommand(
            mCommand->responseTrustlineIsAlreadyPresent());
}

TransactionResult::SharedConst OpenTrustLineTransaction::resultConflictWithOtherOperation() {

    return transactionResultFromCommand(
            mCommand->responseConflictWithOtherOperation());
}

TransactionResult::SharedConst OpenTrustLineTransaction::resultRemoteNodeIsInaccessible() {

    return transactionResultFromCommand(
            mCommand->responseRemoteNodeIsInaccessible());
}

TransactionResult::SharedConst OpenTrustLineTransaction::resultProtocolError() {
    return transactionResultFromCommand(
            mCommand->responseProtocolError());
}

const string OpenTrustLineTransaction::logHeader() const
{
    stringstream s;
    s << "[OpenTrustLineTA: " << currentTransactionUUID() << "]";
    return s.str();
}
