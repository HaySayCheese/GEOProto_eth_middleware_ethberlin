﻿#include "AcceptTrustLineMessage.h"

AcceptTrustLineMessage::AcceptTrustLineMessage(
    BytesShared buffer) {

    deserializeFromBytes(buffer);
}

const Message::MessageType AcceptTrustLineMessage::typeID() const {

    return Message::MessageTypeID::AcceptTrustLineMessageType;
}

const TrustLineAmount &AcceptTrustLineMessage::amount() const {

    return mTrustLineAmount;
}

const size_t AcceptTrustLineMessage::kRequestedBufferSize() {

    static const size_t size = TransactionMessage::kOffsetToInheritedBytes()
                               + kTrustLineAmountBytesCount;

    return size;
}

pair<BytesShared, size_t> AcceptTrustLineMessage::serializeToBytes() {

    auto parentBytesAndCount = TransactionMessage::serializeToBytes();
    size_t bytesCount = parentBytesAndCount.second +
                        kTrustLineAmountBytesCount;

    BytesShared dataBytesShared = tryCalloc(bytesCount);
    size_t dataBytesOffset = 0;
    //----------------------------------------------------
    memcpy(
        dataBytesShared.get(),
        parentBytesAndCount.first.get(),
        parentBytesAndCount.second
    );
    dataBytesOffset += parentBytesAndCount.second;
    //----------------------------------------------------
    vector<byte> buffer = trustLineAmountToBytes(mTrustLineAmount);
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        buffer.data(),
        buffer.size()
    );
    //----------------------------
    return make_pair(
        dataBytesShared,
        bytesCount
    );
}

void AcceptTrustLineMessage::deserializeFromBytes(
    BytesShared buffer) {

    TransactionMessage::deserializeFromBytes(buffer);
    size_t bytesBufferOffset = TransactionMessage::kOffsetToInheritedBytes();
    //----------------------------------------------------
    vector<byte> amountBytes(
        buffer.get() + bytesBufferOffset,
        buffer.get() + bytesBufferOffset + kTrustLineAmountBytesCount);
    mTrustLineAmount = bytesToTrustLineAmount(amountBytes);
}

MessageResult::SharedConst AcceptTrustLineMessage::resultAccepted() const {

    return MessageResult::SharedConst(
        new MessageResult(
            mSenderUUID,
            mTransactionUUID,
            kResultCodeAccepted)
    );
}

MessageResult::SharedConst AcceptTrustLineMessage::resultConflict() const {

    return MessageResult::SharedConst(
        new MessageResult(
            mSenderUUID,
            mTransactionUUID,
            kResultCodeConflict)
    );
}