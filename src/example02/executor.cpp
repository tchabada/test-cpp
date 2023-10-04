#include "executor.hpp"

#include <spdlog/spdlog.h>
#include <chrono>

namespace executor
{

Executor::PeriodicTask::PeriodicTask(Executor::PeriodicTask::PrivateTag,
                                     asio::io_context& ioContext,
                                     Duration          period,
                                     Func              func,
                                     WatchdogManager*  watchdogManager)
  : ioContext_{ioContext}
  , timer_{ioContext}
  , func_{std::move(func)}
  , period_{period}
  , stopped_{true}
  , watchdogManager_{watchdogManager}
  , watchDog_{watchdogManager != nullptr
                  ? std::make_shared<PeriodicWatchdog>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(period * kMaxWatchdogDeviationRatio))
                  : nullptr}
{
}

Executor::PeriodicTask::~PeriodicTask()
{
    try {
        if (watchdogManager_ != nullptr) {
            watchdogManager_->unregisterWatchdog(watchDog_);
        }
    }
    catch (const std::exception& e) {
        spdlog::warn("Executor::PeriodicTask: destructor throws exception {}", e.what());
    }
}

void
Executor::PeriodicTask::start(Duration delay)
{
    bool expected{true};

    if (stopped_.compare_exchange_strong(expected, false)) {
        if (watchdogManager_ != nullptr) {
            watchDog_->deactivate();
            watchdogManager_->registerWatchdog(watchDog_);
        }

        asio::post(ioContext_, [this, delay, self = shared_from_this()]() {
            timer_.expires_from_now(delay);
            timer_.async_wait([this, self = shared_from_this()](const asio::error_code& errorCode) {
                execute(errorCode);
            });
        });
    }
    else {
        spdlog::warn("Executor::PeriodicTask: already started");
    }
}

void
Executor::PeriodicTask::stop()
{
    bool expected{false};

    if (stopped_.compare_exchange_strong(expected, true)) {
        if (watchdogManager_ != nullptr) {
            watchdogManager_->unregisterWatchdog(watchDog_);
        }

        std::lock_guard<std::mutex> lock{mutex_};

        timer_.cancel();
    }
    else {
        spdlog::warn("Executor::PeriodicTask: already stopped");
    }
}

void
Executor::PeriodicTask::runNow()
{
    std::lock_guard<std::mutex> lock{mutex_};

    if (timer_.expires_at(TimePoint::min()) != 0U) {
        timer_.async_wait([this, self = shared_from_this()](const asio::error_code& errorCode) {
            execute(errorCode);
        });
    }
}

void
Executor::PeriodicTask::execute(const asio::error_code& errorCode)
{
    if (!errorCode) {
        auto expire = [this]() -> TimePoint {
            std::lock_guard<std::mutex> lock{mutex_};

            return timer_.expires_at();
        }();

        const auto start = Clock::now();

        if (expire != std::chrono::steady_clock::time_point::min()) {
            const auto deviation = start - expire;

            if (deviation > period_ * kMaxExecuteDeviationRatio) {
                spdlog::warn("Executor::PeriodicTask: time deviation {} us",
                             std::chrono::duration_cast<std::chrono::microseconds>(deviation).count());
            }
        }

        if (watchDog_) {
            watchDog_->tick();
        }

        func_();

        const auto elapsed = Clock::now() - start;

        {
            std::lock_guard<std::mutex> lock{mutex_};

            if (stopped_.load()) {
                return;
            }

            expire = timer_.expires_at();

            if (expire != TimePoint::min()) {
                timer_.expires_from_now(std::clamp(period_ - elapsed, Duration{0}, period_));
            }
            else {
                timer_.expires_from_now(Duration{0});
            }

            timer_.async_wait([this, self = shared_from_this()](const asio::error_code& errorCode) {
                execute(errorCode);
            });
        }
    }
}

Executor::Executor()
  : signalSet_{ioContext_}
  , running_{false}
{
}

void
Executor::run(const size_t threadsCount, std::initializer_list<int> signals)
{
    bool expected{false};

    if (!running_.compare_exchange_strong(expected, true)) {
        spdlog::warn("Executor: already running");

        return;
    }

    spdlog::info("Executor: Started");

    try {
        for (const int signal : signals) {
            signalSet_.add(signal);
        }

        storedException_ = nullptr;

        for (size_t i{0U}; i < threadsCount; ++i) {
            threads_.emplace_back([this]() {
                try {
                    ioContext_.run();
                }
                catch (const std::exception& e) {
                    spdlog::error("Executor: Exception detected: {}", e.what());

                    ioContext_.stop();

                    spdlog::info("Executor: Stopped");

                    storeException();
                }
            });
        }

        ioContext_.run();
    }
    catch (const std::exception& e) {
        spdlog::error("Executor: Exception detected: {}", e.what());

        ioContext_.stop();

        spdlog::info("Executor: Stopped");

        storeException();
    }

    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    signalSet_.clear();
    threads_.clear();

    running_.store(false);

    spdlog::info("Executor: Finished");

    if (storedException_) {
        std::rethrow_exception(storedException_);
    }
}

void
Executor::stop()
{
    ioContext_.stop();
}

void
Executor::restart()
{
    if (running_.load()) {
        return;
    }

    ioContext_.restart();
}

bool
Executor::stopped() const
{
    return ioContext_.stopped();
}

std::shared_ptr<Executor::PeriodicTask>
Executor::createPeriodicTask(const Duration period, PeriodicTask::Func func, bool useWatchdog)
{
    return PeriodicTask::create(ioContext_, period, std::move(func), useWatchdog ? &watchdogManager_ : nullptr);
}

void
Executor::startWatchdogTask(Duration reportPeriod, WatchdogCheckFunc func)
{
    if (!watchDogTask_) {
        watchDogTask_ = createPeriodicTask(reportPeriod, [this, func = std::move(func)]() mutable {
            func(watchdogManager_.check());
        });

        watchDogTask_->start();
    }
}

void
Executor::stopWatchdogTask()
{
    if (watchDogTask_) {
        watchDogTask_->stop();
        watchDogTask_.reset();
    }
}

void
Executor::storeException()
{
    const std::lock_guard<std::mutex> lockGuard{mutex_};

    if (!storedException_) {
        storedException_ = std::current_exception();
    }
}

}  // namespace executor
