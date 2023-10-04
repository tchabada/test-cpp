#pragma once

#include "watchdog.hpp"

#include <asio/io_context.hpp>
#include <asio/post.hpp>
#include <asio/signal_set.hpp>
#include <asio/steady_timer.hpp>
#include <asio/use_future.hpp>

#include <spdlog/spdlog.h>

#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <type_traits>
#include <vector>

namespace executor
{

class Executor
{
public:
    using Clock     = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Duration  = Clock::duration;

    class PeriodicTask final : public std::enable_shared_from_this<PeriodicTask>
    {
    private:
        class PrivateTag;

    public:
        using Func = std::function<void()>;
        // using Func = std::move_only_function<void()>;

        PeriodicTask(
            PrivateTag, asio::io_context& ioContext, Duration period, Func func, WatchdogManager* watchdog = nullptr);

        PeriodicTask(const PeriodicTask&)            = delete;
        PeriodicTask(PeriodicTask&&)                 = delete;
        PeriodicTask& operator=(PeriodicTask&&)      = delete;
        PeriodicTask& operator=(const PeriodicTask&) = delete;

        ~PeriodicTask();

        template <typename... Args>
        static std::shared_ptr<PeriodicTask>
        create(Args&&... args)
        {
            return std::make_shared<PeriodicTask>(PrivateTag{}, std::forward<Args>(args)...);
        }

        void start(Duration delay = std::chrono::microseconds{0});

        void stop();

        void runNow();

    private:
        class PrivateTag final
        {
        public:
            explicit PrivateTag() = default;
        };

        void execute(const asio::error_code& errorCode);

        asio::io_context&                 ioContext_;
        std::mutex                        mutex_;
        asio::steady_timer                timer_;
        Func                              func_;
        Duration                          period_;
        std::atomic<bool>                 stopped_;
        WatchdogManager*                  watchdogManager_;
        std::shared_ptr<PeriodicWatchdog> watchDog_;

        static constexpr float kMaxExecuteDeviationRatio{0.001F};
        static constexpr float kMaxWatchdogDeviationRatio{2.0F};
    };

    Executor();

    virtual ~Executor() = default;

    Executor(const Executor&)            = delete;
    Executor& operator=(const Executor&) = delete;
    Executor(Executor&&)                 = delete;
    Executor& operator=(Executor&&)      = delete;

    void run(size_t threadsCount = 0U, std::initializer_list<int> signals = {});

    template <typename F>
    void
    spawn(Duration timeout, F&& func)
    {
        auto watchDog = std::make_shared<IntervalWatchdog>(timeout);

        watchDog->tick();

        watchdogManager_.registerWatchdog(watchDog);

        asio::post(ioContext_, [this, watchDog, func = std::forward<F>(func)]() {
            func();
            watchdogManager_.unregisterWatchdog(watchDog);
        });
    }

    template <typename F>
    auto
    post(Duration timeout, F&& func) -> std::future<decltype(func())>
    {
        auto watchDog = std::make_shared<IntervalWatchdog>(timeout);

        watchDog->tick();

        watchdogManager_.registerWatchdog(watchDog);

        return asio::post(
            ioContext_, asio::use_future([this, watchDog, func = std::forward<F>(func)]() -> decltype(auto) {
                const auto unregisterWatcgDog = [this, &watchDog](void*) {
                    watchdogManager_.unregisterWatchdog(watchDog);
                };

                const std::unique_ptr<void, decltype(unregisterWatcgDog)> onExit{this, unregisterWatcgDog};

                return func();
            }));
    }

    std::shared_ptr<PeriodicTask> createPeriodicTask(Duration period, PeriodicTask::Func func, bool useWatchdog = true);

    void stop();

    void restart();

    bool stopped() const;

    using WatchdogCheckFunc = std::function<void(bool)>;
    // using WatchdogCheckFunc = std::move_only_function<void(bool)>;

    void startWatchdogTask(Duration reportPeriod, WatchdogCheckFunc func);

    void stopWatchdogTask();

    template <typename SignalHandler>
    void
    addSignalHandler(SignalHandler&& signalHandler)
    {
        signalSet_.async_wait(std::forward<SignalHandler>(signalHandler));
    }

private:
    void storeException();

private:
    asio::io_context              ioContext_;
    asio::signal_set              signalSet_;
    std::vector<std::thread>      threads_;
    std::mutex                    mutex_;
    std::exception_ptr            storedException_;
    WatchdogManager               watchdogManager_;
    std::shared_ptr<PeriodicTask> watchDogTask_;
    std::atomic<bool>             running_;
};

}  // namespace executor
