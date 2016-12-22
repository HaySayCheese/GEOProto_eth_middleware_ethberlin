#ifndef GEO_NETWORK_CLIENT_UPDATETRUSTLINETRANSACTION_H
#define GEO_NETWORK_CLIENT_UPDATETRUSTLINETRANSACTION_H

#include "BaseTransaction.h"
#include "../../interface/commands/commands/UpdateTrustLineCommand.h"

class TransactionsScheduler;

class UpdateTrustLineTransaction : public BaseTransaction {
    friend class TransactionsScheduler;

private:
    UpdateTrustLineCommand::Shared mCommand;

public:
    UpdateTrustLineTransaction(
            UpdateTrustLineCommand::Shared command);

private:
    UpdateTrustLineCommand::Shared command() const;

    void setContext(
            Message::Shared message);

    pair<CommandResult::SharedConst, TransactionState::SharedConst> run();

};


#endif //GEO_NETWORK_CLIENT_UPDATETRUSTLINETRANSACTION_H
