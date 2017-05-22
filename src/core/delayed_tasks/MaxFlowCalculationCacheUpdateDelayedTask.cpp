#include "MaxFlowCalculationCacheUpdateDelayedTask.h"

MaxFlowCalculationCacheUpdateDelayedTask::MaxFlowCalculationCacheUpdateDelayedTask(
    as::io_service &ioService,
    MaxFlowCalculationCacheManager *maxFlowCalculationCacheMnager,
    MaxFlowCalculationTrustLineManager *maxFlowCalculationTrustLineManager,
    Logger *logger):

    mIOService(ioService),
    mMaxFlowCalculationCacheMnager(maxFlowCalculationCacheMnager),
    mMaxFlowCalculationTrustLineManager(maxFlowCalculationTrustLineManager),
    mLog(logger)
{
    mMaxFlowCalculationCacheUpdateTimer = make_unique<as::steady_timer>(
        mIOService);

    Duration microsecondsDelay = minimalAwakeningTimestamp() - utc_now();
    mMaxFlowCalculationCacheUpdateTimer->expires_from_now(
        chrono::milliseconds(
            microsecondsDelay.total_milliseconds()));
    mMaxFlowCalculationCacheUpdateTimer->async_wait(boost::bind(
        &MaxFlowCalculationCacheUpdateDelayedTask::runSignalMaxFlowCalculationCacheUpdate,
        this,
        as::placeholders::error));
}

void MaxFlowCalculationCacheUpdateDelayedTask::runSignalMaxFlowCalculationCacheUpdate(
    const boost::system::error_code &errorCode)
{
    if (errorCode) {
        error() << errorCode.message().c_str();
    }
    updateCache();
    Duration microsecondsDelay = minimalAwakeningTimestamp() - utc_now();
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    auto duration = chrono::milliseconds(microsecondsDelay.total_milliseconds());
    debug() << "next launch: " << duration.count() << " ms" << endl;
#endif
    mMaxFlowCalculationCacheUpdateTimer->expires_from_now(
        chrono::milliseconds(
            microsecondsDelay.total_milliseconds()));
    mMaxFlowCalculationCacheUpdateTimer->async_wait(boost::bind(
        &MaxFlowCalculationCacheUpdateDelayedTask::runSignalMaxFlowCalculationCacheUpdate,
        this,
        as::placeholders::error));
}

DateTime MaxFlowCalculationCacheUpdateDelayedTask::minimalAwakeningTimestamp()
{
    DateTime closestCacheManagerTimeEvent = mMaxFlowCalculationCacheMnager->closestTimeEvent();
    DateTime closestTrustLineManagerTimeEvent = mMaxFlowCalculationTrustLineManager->closestTimeEvent();
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    debug() << "minimalAwakeningTimestamp Cache Manager closest time: "
         <<  closestCacheManagerTimeEvent << endl;
    debug() << "minimalAwakeningTimestamp TrustLine Manager closest time: "
         <<  closestTrustLineManagerTimeEvent << endl;
#endif
    if (closestCacheManagerTimeEvent < closestTrustLineManagerTimeEvent) {
        return closestCacheManagerTimeEvent;
    } else {
        return closestTrustLineManagerTimeEvent;
    }
}

void MaxFlowCalculationCacheUpdateDelayedTask::updateCache()
{
    mMaxFlowCalculationCacheMnager->updateCaches();
    mMaxFlowCalculationTrustLineManager->deleteLegacyTrustLines();
}

LoggerStream MaxFlowCalculationCacheUpdateDelayedTask::debug() const
{
    if (nullptr == mLog)
        throw Exception("logger is not initialised");
    return mLog->debug(logHeader());
}

LoggerStream MaxFlowCalculationCacheUpdateDelayedTask::info() const
{
    if (nullptr == mLog)
        throw Exception("logger is not initialised");
    return mLog->info(logHeader());
}

LoggerStream MaxFlowCalculationCacheUpdateDelayedTask::error() const
{
    if (nullptr == mLog)
        throw Exception("logger is not initialised");
    return mLog->error(logHeader());
}

const string MaxFlowCalculationCacheUpdateDelayedTask::logHeader() const
{
    return "[MaxFlowCalculationCacheUpdateDelayedTask]";
}
