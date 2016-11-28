#ifndef GEO_NETWORK_CLIENT_CLOSETRUSTLINECOMMAND_H
#define GEO_NETWORK_CLIENT_CLOSETRUSTLINECOMMAND_H

#include "Command.h"

class CloseTrustLineCommand : public Command {
private:
    string mCommandBuffer;
    uuids::uuid mContractorUUID;

public:
    CloseTrustLineCommand(const uuids::uuid &commandUUID, const string &identifier,
                         const string &timestampExcepted, string &commandBuffer);

    const uuids::uuid &commandUUID() const;

    const string &id() const;

    const string &exceptedTimestamp() const;

    const uuids::uuid &contractorUUID() const;

private:
    void deserialize();
};

#endif //GEO_NETWORK_CLIENT_CLOSETRUSTLINECOMMAND_H
