#include "example02/executor.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <csignal>
#include <random>

using namespace std::chrono_literals;

namespace executor
{

class ExecutorTest : public testing::Test
{
public:
    void
    SetUp() override
    {
    }

    MOCK_METHOD(void, tick, ());
};

TEST_F(ExecutorTest, spawnTest)
{
    EXPECT_CALL(*this, tick()).Times(1);

    Executor executor;

    executor.spawn(500ms, [this]() {
        this->tick();
    });

    ASSERT_NO_THROW(executor.run());
}

TEST_F(ExecutorTest, spawnWithExceptionTest)
{
    Executor executor;

    executor.spawn(500ms, []() {
        throw std::runtime_error("");
    });

    ASSERT_THROW(executor.run(), std::runtime_error);
}

TEST_F(ExecutorTest, postWithFutureTest)
{
    Executor executor;

    auto f = executor.post(500ms, []() -> int {
        const int result = 42;
        return result;
    });

    ASSERT_NO_THROW(executor.run());

    ASSERT_EQ(f.get(), 42);
}

TEST_F(ExecutorTest, postWithFutureVoidTest)
{
    Executor executor;

    auto f = executor.post(500ms, []() {
    });

    ASSERT_NO_THROW(executor.run());

    ASSERT_NO_THROW(f.get());
}

TEST_F(ExecutorTest, createPeriodicTaskTest)
{
    constexpr size_t kCallCount = 10U;

    EXPECT_CALL(*this, tick()).Times(kCallCount);

    using namespace std::chrono_literals;

    Executor executor;

    std::shared_ptr<Executor::PeriodicTask> task = executor.createPeriodicTask(
        10ms,
        [&task, this]() {
            this->tick();

            static size_t c = 0U;

            if (++c == kCallCount) {
                task->stop();
                // second stop should do nothing
                task->stop();
            }
        },
        false);

    task->start();

    // second start should do nothing
    task->start();

    ASSERT_NO_THROW(executor.run());

    EXPECT_CALL(*this, tick()).Times(kCallCount);

    executor
        .createPeriodicTask(10ms,
                            [&executor, this]() {
                                this->tick();

                                static size_t c = 0U;

                                if (++c == kCallCount) {
                                    executor.stop();
                                }
                            })
        ->start();

    executor.restart();
    ASSERT_NO_THROW(executor.run());
}

TEST_F(ExecutorTest, createPeriodicTaskTestWithWatchdog)
{
    using namespace std::chrono_literals;

    Executor                                executor;
    std::shared_ptr<Executor::PeriodicTask> task = executor.createPeriodicTask(
        10ms,
        [&task]() {
            task->stop();

            // second stop should do nothing
            task->stop();
        },
        true);

    task->start();

    ASSERT_NO_THROW(executor.run());
}

TEST_F(ExecutorTest, createPeriodicTaskRunNowTest)
{
    constexpr size_t kCallCount = 2U;

    EXPECT_CALL(*this, tick()).Times(kCallCount);

    using namespace std::chrono_literals;

    Executor          executor;
    std::atomic<bool> done{false};

    std::shared_ptr<Executor::PeriodicTask> task = executor.createPeriodicTask(10s, [&done, &executor, this]() {
        this->tick();

        static size_t c = 0U;

        if (++c == kCallCount) {
            executor.stop();
        }

        done.store(true);
    });

    // runNow before executor start does nothing
    task->runNow();

    auto nowTs = std::chrono::steady_clock::now();

    // start task and run executor
    task->start();

    auto taskFuture = std::async([&executor] {
        executor.run();
    });

    // wait for the first run
    while (!done.load()) {
        std::this_thread::sleep_for(1ms);
    }

    std::this_thread::sleep_for(1ms);

    // executor is running, so run should do nothing
    executor.run();

    // run task now
    task->runNow();

    // wait for executor to finish
    taskFuture.wait();

    EXPECT_LT(std::chrono::steady_clock::now() - nowTs, 5s);
}

TEST_F(ExecutorTest, addSignalHandlerTest)
{
    EXPECT_CALL(*this, tick()).Times(1);

    using namespace std::chrono_literals;

    Executor executor;

    executor.addSignalHandler([this](const asio::error_code&, int32_t) {
        this->tick();
    });

    executor.spawn(500ms, []() {
        std::raise(SIGTERM);
    });

    ASSERT_NO_THROW(executor.run(0, {SIGTERM}));
}

TEST_F(ExecutorTest, taskWithException)
{
    EXPECT_CALL(*this, tick()).Times(2);

    Executor          executor;
    std::atomic<bool> task1started{false};
    std::atomic<bool> task2done{false};

    using namespace std::chrono_literals;

    std::shared_ptr<Executor::PeriodicTask> task1
        = executor.createPeriodicTask(100ms, [this, &task1started, &task2done]() {
              this->tick();

              task1started.store(true);

              while (!task2done.load()) {
                  std::this_thread::sleep_for(1ms);
              }

              std::this_thread::sleep_for(5ms);

              throw std::runtime_error("1");
          });

    std::shared_ptr<Executor::PeriodicTask> task2
        = executor.createPeriodicTask(100ms, [this, &task1started, &task2done]() {
              this->tick();

              while (!task1started.load()) {
                  std::this_thread::sleep_for(1ms);
              }

              task2done.store(true);

              throw std::logic_error("2");
          });

    // start task and run executor
    task1->start(5ms);
    task2->start(10ms);

    // the first exception should be raised from task2 -> std::logic_error
    ASSERT_THROW(executor.run(1U), std::logic_error);
}

TEST_F(ExecutorTest, stopBeforeRunTest)
{
    Executor executor;

    ASSERT_EQ(false, executor.stopped());
    executor.stop();
    ASSERT_EQ(true, executor.stopped());

    bool executed = false;

    executor.spawn(500ms, [&executed]() {
        executed = true;
    });

    ASSERT_NO_THROW(executor.run());
    ASSERT_FALSE(executed);
}

TEST_F(ExecutorTest, restartTest)
{
    Executor executor;

    ASSERT_NO_THROW(executor.run());

    size_t count = 0U;

    executor.spawn(500ms, [&count]() {
        count++;
    });

    ASSERT_NO_THROW(executor.run());
    ASSERT_EQ(count, 0U);  // executor is stopped after run() call

    executor.restart();

    ASSERT_NO_THROW(executor.run());  // executor run queued task after restart
    ASSERT_EQ(count, 1U);
}

TEST_F(ExecutorTest, watchdogTaskTest)
{
    Executor executor;

    executor.addSignalHandler([&executor](const asio::error_code&, int32_t signalNumber) {
        spdlog::info("received signal number {} stopping", signalNumber);

        executor.stop();
    });

    std::random_device                                       dev;
    std::mt19937                                             gen(dev());
    std::uniform_int_distribution<std::mt19937::result_type> distrib(100, 220);

    auto task{executor.createPeriodicTask(std::chrono::milliseconds{100}, [&gen, &distrib]() {
        auto start = std::chrono::steady_clock::now();

        std::this_thread::sleep_for(std::chrono::milliseconds{distrib(gen)});

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);

        spdlog::info("elapsed {}ms", std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
    })};

    task->start();

    executor.startWatchdogTask(std::chrono::milliseconds{10}, [&executor](bool status) {
        if (!status) {
            spdlog::warn("watchdog check failed stopping");

            executor.stop();
        }
    });

    executor.run(4U, {SIGINT, SIGTERM});

    executor.stopWatchdogTask();

    task->stop();
}

}  // namespace executor
