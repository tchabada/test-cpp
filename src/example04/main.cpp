#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

#include <print>

int
main()
{
    exec::static_thread_pool pool(3);

    auto sched = pool.get_scheduler();

    auto func = [](int i) {
        return i * i;
    };

    auto work = stdexec::when_all(stdexec::on(sched, stdexec::just(0) | stdexec::then(func)),
                                  stdexec::on(sched, stdexec::just(1) | stdexec::then(func)),
                                  stdexec::on(sched, stdexec::just(2) | stdexec::then(func)));

    auto [i, j, k] = stdexec::sync_wait(work).value();

    std::println("{} {} {}", i, j, k);
}
