#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

using namespace exec;
using namespace stdexec;

int main()
{
    static_thread_pool pool(3);

    auto sched = pool.get_scheduler();

    auto fun = [](int i) { return i * i; };
    auto work =
        when_all(on(sched, just(0) | then(fun)), on(sched, just(1) | then(fun)), on(sched, just(2) | then(fun)));

    auto [i, j, k] = sync_wait(std::move(work)).value();

    std::printf("%d %d %d\n", i, j, k);
}
