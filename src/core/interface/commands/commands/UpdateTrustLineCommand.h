#ifndef GEO_NETWORK_CLIENT_UPDATEOUTGOINGTRUSTAMOUNTCOMMAND_H
#define GEO_NETWORK_CLIENT_UPDATEOUTGOINGTRUSTAMOUNTCOMMAND_H

#include "BaseUserCommand.h"
#include "../../../common/exceptions/ValueError.h"
#include "../../../trust_lines/TrustLine.h"


class UpdateTrustLineCommand:
    public BaseUserCommand {

public:
    UpdateTrustLineCommand(
        const CommandUUID &uuid,
        const string &commandBuffer);

    static const string& identifier();
    const NodeUUID &contractorUUID() const;
    const trust_amount &amount() const;

protected:
    NodeUUID mContractorUUID;
    trust_amount mAmount;

protected:
    void deserialize(
        const string &command);
};

#endif //GEO_NETWORK_CLIENT_UPDATEOUTGOINGTRUSTAMOUNTCOMMAND_H