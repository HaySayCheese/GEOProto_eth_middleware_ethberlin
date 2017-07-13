#include "PaymentRecord.h"

PaymentRecord::PaymentRecord(
    const TransactionUUID &operationUUID,
    const PaymentRecord::PaymentOperationType operationType,
    const NodeUUID &contractorUUID,
    const TrustLineAmount &amount,
    const TrustLineBalance &balanceAfterOperation):

    Record(
        Record::RecordType::PaymentRecordType,
        operationUUID,
        contractorUUID),
    mPaymentOperationType(operationType),
    mAmount(amount),
    mBalanceAfterOperation(balanceAfterOperation)
{}

PaymentRecord::PaymentRecord(
    const TransactionUUID &operationUUID,
    const PaymentRecord::PaymentOperationType operationType,
    const NodeUUID &contractorUUID,
    const TrustLineAmount &amount,
    const TrustLineBalance &balanceAfterOperation,
    const GEOEpochTimestamp geoEpochTimestamp):

    Record(
        Record::RecordType::PaymentRecordType,
        operationUUID,
        contractorUUID,
        geoEpochTimestamp),
    mPaymentOperationType(operationType),
    mAmount(amount),
    mBalanceAfterOperation(balanceAfterOperation)
{}

PaymentRecord::PaymentRecord(
    const TransactionUUID &operationUUID,
    const PaymentRecord::PaymentOperationType operationType,
    const TrustLineAmount &amount):

    Record(
        Record::RecordType::PaymentRecordType,
        operationUUID,
        NodeUUID::empty()),
    mPaymentOperationType(operationType),
    mAmount(amount)
{}

PaymentRecord::PaymentRecord(
    const TransactionUUID &operationUUID,
    const PaymentRecord::PaymentOperationType operationType,
    const TrustLineAmount &amount,
    const GEOEpochTimestamp geoEpochTimestamp):

    Record(
        Record::RecordType::PaymentRecordType,
        operationUUID,
        NodeUUID::empty(),
        geoEpochTimestamp),
    mPaymentOperationType(operationType),
    mAmount(amount)
{}

const bool PaymentRecord::isPaymentRecord() const
{
    return true;
}

const PaymentRecord::PaymentOperationType PaymentRecord::paymentOperationType() const
{
    return mPaymentOperationType;
}

const TrustLineAmount PaymentRecord::amount() const
{
    return mAmount;
}

const TrustLineBalance PaymentRecord::balanceAfterOperation() const
{
    return mBalanceAfterOperation;
}