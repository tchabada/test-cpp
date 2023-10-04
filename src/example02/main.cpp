#include "executor.hpp"

#include "example01/machine.hpp"

#include <spdlog/spdlog.h>

#include <chrono>
#include <exception>
#include <random>

class Application final
{
public:
    Application()
      : running_{false}
    {
        executor_.addSignalHandler([&executor = executor_](const asio::error_code&, int32_t signalNumber) {
            spdlog::info("Application: received signal number {} stopping", signalNumber);

            executor.stop();
        });
    };

    ~Application() = default;

    Application(const Application&)            = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&)                 = delete;
    Application& operator=(Application&&)      = delete;

    void
    run()
    {
        bool expected{false};

        if (!running_.compare_exchange_strong(expected, true)) {
            spdlog::warn("Application: already running");

            return;
        }

        std::random_device                                       dev;
        std::mt19937                                             gen(dev());
        std::uniform_int_distribution<std::mt19937::result_type> distrib{1, 10};

        auto task{executor_.createPeriodicTask(std::chrono::milliseconds{10}, [&gen, &distrib]() {
            auto start = std::chrono::steady_clock::now();

            std::this_thread::sleep_for(std::chrono::milliseconds{distrib(gen)});

            auto elapsed
                = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);

            spdlog::info("Application: elapsed {} ms",
                         std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
        })};

        task->start();

        executor_.startWatchdogTask(std::chrono::milliseconds{10}, [](bool status) {
            if (!status) {
                spdlog::warn("Application: watchdog check failed");
            }
        });

        executor_.run(4U, {SIGINT, SIGTERM});

        executor_.stopWatchdogTask();

        task->stop();

        task.reset();

        running_.store(false);
    }

    void
    stop()
    {
        executor_.stop();
    }

private:
    executor::Executor executor_;
    std::atomic<bool>  running_;
};

int
main()
{
    try {
        Application appplication;

        appplication.run();
    }
    catch (const std::exception& e) {
        spdlog::error("Exception: {}", e.what());
    }

    return 0;
}