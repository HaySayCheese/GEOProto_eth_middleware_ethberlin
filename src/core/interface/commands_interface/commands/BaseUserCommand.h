﻿#ifndef GEO_NETWORK_CLIENT_COMMAND_H
#define GEO_NETWORK_CLIENT_COMMAND_H

#include "../../../common/Types.h"
#include "../../../common/time/TimeUtils.h"
#include "../../../common/memory/MemoryUtils.h"

#include "../../results_interface/result/CommandResult.h"

#include <boost/uuid/uuid.hpp>


namespace uuids = boost::uuids;

class BaseUserCommand {
public:
    typedef shared_ptr<BaseUserCommand> Shared;

public:
    static const constexpr char kCommandsSeparator = '\n';
    static const constexpr char kTokensSeparator = '\t';

public:
    BaseUserCommand(
        const string& identifier);

    BaseUserCommand(
        const CommandUUID &commandUUID,
        const string& identifier);

    virtual ~BaseUserCommand() = default;

    const CommandUUID &UUID() const;

    const string &identifier() const;

    const DateTime &timestampAccepted() const;

    // TODO: remove noexcept
    // TODO: split methods into classes
    CommandResult::SharedConst responseOK() const
        noexcept;
    CommandResult::SharedConst responseCreated() const
        noexcept;
    CommandResult::SharedConst responsePostponedByReservations() const
        noexcept;
    CommandResult::SharedConst responseProtocolError() const
        noexcept;
    CommandResult::SharedConst responseTrustlineIsAbsent() const
        noexcept;
    CommandResult::SharedConst responseCurrentIncomingDebtIsGreaterThanNewAmount() const
        noexcept;
    CommandResult::SharedConst responseTrustlineIsAlreadyPresent() const
        noexcept;

    CommandResult::SharedConst responseTrustlineRejected() const;

    CommandResult::SharedConst responseInsufficientFunds() const
        noexcept;
    CommandResult::SharedConst responseConflictWithOtherOperation() const
        noexcept;
    CommandResult::SharedConst responseRemoteNodeIsInaccessible() const
        noexcept;
    CommandResult::SharedConst responseNoRoutes() const
        noexcept;
    CommandResult::SharedConst responseUnexpectedError() const
        noexcept;
    // this response used during disabling start payment and trust line transactions
    CommandResult::SharedConst responseForbiddenRunTransaction() const
        noexcept;

protected:
    virtual pair<BytesShared, size_t> serializeToBytes();

    virtual void deserializeFromBytes(
        BytesShared buffer);

    [[deprecated("move parse logic into the command's constructor")]]
    virtual void parse(
        const string &commandBuffer) = 0;

    static const size_t kOffsetToInheritedBytes();

    CommandResult::SharedConst makeResult(
        const uint16_t code) const;

private:
    const CommandUUID mCommandUUID;
    DateTime mTimestampAccepted;
    const string mCommandIdentifier;
};
#endif //GEO_NETWORK_CLIENT_COMMAND_H