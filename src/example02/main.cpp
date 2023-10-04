
#include <chrono>
#include <iostream>

#include <unifex/scheduler_concepts.hpp>
#include <unifex/sync_wait.hpp>
#include <unifex/task.hpp>
#include <unifex/timed_single_thread_context.hpp>

using namespace unifex;
using namespace std::chrono;
using namespace std::chrono_literals;
using namespace std;

timed_single_thread_context timer;

auto delay(milliseconds ms) { return schedule_after(timer.get_scheduler(), ms); }

unifex::task<void> asyncMain() { co_await delay(1000ms); }

int main()
{
    auto start_time = steady_clock::now();
    sync_wait(asyncMain());
    cout << "Total time is: " << duration_cast<std::chrono::milliseconds>(steady_clock::now() - start_time).count()
         << "ms\n";
    return 0;
}
