
#include <chrono>
#include <iostream>

#include <unifex/scheduler_concepts.hpp>
#include <unifex/sync_wait.hpp>
#include <unifex/task.hpp>
#include <unifex/timed_single_thread_context.hpp>

namespace
{
using namespace std::chrono_literals;

unifex::timed_single_thread_context timer;

auto
delay(std::chrono::milliseconds amout)
{
    return unifex::schedule_after(timer.get_scheduler(), amout);
}

unifex::task<void>
asyncMain()
{
    co_await delay(1000ms);
}
}  // namespace

int
main()
{
    auto startTime = std::chrono::steady_clock::now();

    unifex::sync_wait(asyncMain());

    std::cout << "Total time is: "
              << duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count()
              << "ms\n";

    return 0;
}
