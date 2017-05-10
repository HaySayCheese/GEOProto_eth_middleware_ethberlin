#ifndef GEO_NETWORK_CLIENT_PAYMENTOPERATIONSTATEHANDLER_H
#define GEO_NETWORK_CLIENT_PAYMENTOPERATIONSTATEHANDLER_H

#include "../../logger/Logger.h"
#include "../../transactions/transactions/base/TransactionUUID.h"
#include "../../common/Types.h"
#include "../../common/memory/MemoryUtils.h"
#include "../../common/exceptions/IOError.h"
#include "../../common/exceptions/NotFoundError.h"

#include "../../../libs/sqlite3/sqlite3.h"

class PaymentOperationStateHandler {

public:
    PaymentOperationStateHandler(
        sqlite3 *dbConnection,
        const string &tableName,
        Logger *logger);

    void saveRecord(
        const TransactionUUID &transactionUUID,
        BytesShared state,
        size_t stateBytesCount);

    pair<BytesShared, size_t> getState(
        const TransactionUUID &transactionUUID);

    void deleteRecord(
        const TransactionUUID &transactionUUID);

private:
    LoggerStream info() const;

    LoggerStream error() const;

    const string logHeader() const;

private:
    sqlite3 *mDataBase = nullptr;
    string mTableName;
    Logger *mLog;
};


#endif //GEO_NETWORK_CLIENT_PAYMENTOPERATIONSTATEHANDLER_H
