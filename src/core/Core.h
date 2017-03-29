﻿
#ifndef GEO_NETWORK_CLIENT_CORE_H
#define GEO_NETWORK_CLIENT_CORE_H

#include "common/Types.h"
#include "common/NodeUUID.h"

#include "settings/Settings.h"
#include "db/operations_history_storage/storage/OperationsHistoryStorage.h"
#include "network/communicator/Communicator.h"
#include "interface/commands_interface/interface/CommandsInterface.h"
#include "interface/results_interface/interface/ResultsInterface.h"
#include "trust_lines/manager/TrustLinesManager.h"
#include "resources/manager/ResourcesManager.h"
#include "transactions/manager/TransactionsManager.h"
#include "delayed_tasks/Cycles.h"
#include "max_flow_calculation/manager/MaxFlowCalculationTrustLineManager.h"
#include "max_flow_calculation/cashe/MaxFlowCalculationCacheManager.h"
#include "delayed_tasks/MaxFlowCalculationCacheUpdateDelayedTask.h"
#include "io/storage/StorageHandler.h"
#include "paths/PathsManager.h"

#include "logger/Logger.h"

#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <boost/signals2.hpp>

using namespace std;

namespace as = boost::asio;
namespace signals = boost::signals2;
namespace history = db::operations_history_storage;

class Core {

public:
    Core();

    ~Core();

    int run();

private:
    int initCoreComponents();

    int initSettings();

    int initOperationsHistoryStorage();

    int initCommunicator(
        const json &conf);

    int initCommandsInterface();

    int initResultsInterface();

    int initTrustLinesManager();

    int initMaxFlowCalculationTrustLineManager();

    int initMaxFlowCalculationCacheManager();

    int initResourcesManager();

    int initTransactionsManager();

    int initDelayedTasks();

    int initStorageHandler();

    int initPathsManager();

    void connectCommunicatorSignals();

    void connectTrustLinesManagerSignals();

    void connectDelayedTasksSignals();

    void connectResourcesManagerSignals();

    void connectSignalsToSlots();

    void onMessageReceivedSlot(
        Message::Shared message);

    void onMessageSendSlot(
        Message::Shared message,
        const NodeUUID &contractorUUID);

    void onTrustLineCreatedSlot(
        const NodeUUID &contractorUUID, 
        const TrustLineDirection direction);

    void onTrustLineStateModifiedSlot(
        const NodeUUID &contractorUUID,
        const TrustLineDirection direction);

    void onDelayedTaskCycleSixNodesSlot();

    void onDelayedTaskCycleFiveNodesSlot();

    void onPathsResourceRequestedSlot(
        const TransactionUUID &transactionUUID,
        const NodeUUID &destinationNodeUUID);

    void onResourceCollectedSlot(
        BaseResource::Shared resource);

    void zeroPointers();

    void cleanupMemory();

    void writePIDFile();

protected:
    Logger mLog;

    NodeUUID mNodeUUID;
    as::io_service mIOService;

    Settings *mSettings;
    history::OperationsHistoryStorage *mOperationsHistoryStorage;
    Communicator *mCommunicator;
    CommandsInterface *mCommandsInterface;
    ResultsInterface *mResultsInterface;
    TrustLinesManager *mTrustLinesManager;
    ResourcesManager *mResourcesManager;
    TransactionsManager *mTransactionsManager;
    CyclesDelayedTasks *mCyclesDelayedTasks;
    MaxFlowCalculationTrustLineManager *mMaxFlowCalculationTrustLimeManager;
    MaxFlowCalculationCacheManager *mMaxFlowCalculationCacheManager;
    MaxFlowCalculationCacheUpdateDelayedTask *mMaxFlowCalculationCacheUpdateDelayedTask;
    StorageHandler *mStorageHandler;
    PathsManager *mPathsManager;
};

#endif //GEO_NETWORK_CLIENT_CORE_H
