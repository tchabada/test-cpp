#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <vector>

namespace executor
{

class IWatchdog
{
public:
    using Clock          = std::chrono::steady_clock;
    using TimePoint      = Clock::time_point;
    using Duration       = Clock::duration;
    using TimeSinceEpoch = decltype(Clock::now().time_since_epoch().count());

    virtual ~IWatchdog() = default;

    virtual bool check(TimePoint timePoint) const = 0;

protected:
    IWatchdog()                              = default;
    IWatchdog(const IWatchdog&)              = default;
    IWatchdog& operator=(const IWatchdog&) & = default;
    IWatchdog(IWatchdog&&)                   = default;
    IWatchdog& operator=(IWatchdog&&) &      = default;
};

class PeriodicWatchdog final : public IWatchdog
{
public:
    explicit PeriodicWatchdog(Duration duration);

    ~PeriodicWatchdog() final = default;

    PeriodicWatchdog(const PeriodicWatchdog&)            = delete;
    PeriodicWatchdog(PeriodicWatchdog&&)                 = delete;
    PeriodicWatchdog& operator=(PeriodicWatchdog&&)      = delete;
    PeriodicWatchdog& operator=(const PeriodicWatchdog&) = delete;

    bool check(TimePoint timePoint) const final;

    void tick();

    void deactivate();

private:
    Duration                    duration_;
    std::atomic<TimeSinceEpoch> tick_;
    std::atomic<bool>           active_;
};

class IntervalWatchdog final : public IWatchdog
{
public:
    explicit IntervalWatchdog(Duration duration);

    ~IntervalWatchdog() final = default;

    IntervalWatchdog(const IntervalWatchdog&)            = delete;
    IntervalWatchdog(IntervalWatchdog&&)                 = delete;
    IntervalWatchdog& operator=(IntervalWatchdog&&)      = delete;
    IntervalWatchdog& operator=(const IntervalWatchdog&) = delete;

    bool check(TimePoint timePoint) const final;

    void tick();

    void tock();

    void deactivate();

private:
    Duration                    duration_;
    std::atomic<TimeSinceEpoch> tick_;
    std::atomic<TimeSinceEpoch> tock_;
};

class WatchdogManager final
{
public:
    explicit WatchdogManager();

    ~WatchdogManager();

    WatchdogManager(WatchdogManager const&)            = delete;
    WatchdogManager& operator=(WatchdogManager const&) = delete;
    WatchdogManager(WatchdogManager&&)                 = delete;
    WatchdogManager& operator=(WatchdogManager&&)      = delete;

    bool check();

    void registerWatchdog(const std::shared_ptr<IWatchdog>& watchdog);

    void unregisterWatchdog(const std::shared_ptr<IWatchdog>& watchdog);

private:
    std::mutex                              mutex_;
    std::vector<std::shared_ptr<IWatchdog>> watchdogs_;
};

}  // namespace executor
