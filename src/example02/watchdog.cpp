#include "watchdog.hpp"

#include <algorithm>

namespace executor
{

PeriodicWatchdog::PeriodicWatchdog(const Duration duration)
  : duration_{duration}
  , tick_{0}
  , active_{true}
{
}

bool
PeriodicWatchdog::check(const TimePoint timePoint) const
{
    return !active_.load() || (timePoint - TimePoint{Duration{tick_.load()}} <= duration_);
}

void
PeriodicWatchdog::tick()
{
    active_.store(true);
    tick_.store(Clock::now().time_since_epoch().count());
}

void
PeriodicWatchdog::deactivate()
{
    active_.store(false);
}

IntervalWatchdog::IntervalWatchdog(const Duration duration)
  : duration_{duration}
  , tick_{0}
  , tock_{-1}
{
}

bool
IntervalWatchdog::check(const TimePoint timePoint) const
{
    const TimePoint tick{Duration{tick_.load()}};
    const TimePoint tock{Duration{tock_.load()}};

    return (tock >= tick ? tock : timePoint) - tick <= duration_;
}

void
IntervalWatchdog::tick()
{
    tick_.store(Clock::now().time_since_epoch().count());
}

void
IntervalWatchdog::tock()
{
    tock_.store(Clock::now().time_since_epoch().count());
}

void
IntervalWatchdog::deactivate()
{
    const auto now = Clock::now().time_since_epoch().count();

    tick_.store(now);
    tock_.store(now);
}

WatchdogManager::WatchdogManager() = default;

WatchdogManager::~WatchdogManager() = default;

bool
WatchdogManager::check()
{
    const std::lock_guard<std::mutex> lockGuard(mutex_);

    return std::all_of(std::cbegin(watchdogs_),
                       std::cend(watchdogs_),
                       [timePoint = IWatchdog::Clock::now()](const auto& watchdog) -> bool {
                           return watchdog->check(timePoint);
                       });
}

void
WatchdogManager::registerWatchdog(const std::shared_ptr<IWatchdog>& watchdog)
{
    const std::lock_guard<std::mutex> lockGuard(mutex_);

    if (std::find(watchdogs_.begin(), watchdogs_.end(), watchdog) == watchdogs_.end()) {
        watchdogs_.push_back(watchdog);
    }
}

void
WatchdogManager::unregisterWatchdog(const std::shared_ptr<IWatchdog>& watchdog)
{
    const std::lock_guard<std::mutex> lockGuard(mutex_);

    watchdogs_.erase(std::remove(watchdogs_.begin(), watchdogs_.end(), watchdog), watchdogs_.cend());
}

}  // namespace executor
