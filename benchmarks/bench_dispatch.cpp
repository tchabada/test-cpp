#include "dispatch_context.hpp"

#include <benchmark/benchmark.h>
#include <boost/poly_collection/base_collection.hpp>
#include <boost/poly_collection/variant_collection.hpp>

#include <memory>
#include <utility>
#include <variant>
#include <vector>

namespace
{
void
BENCHMARK_VirtualCallInline(benchmark::State& state)
{
    constexpr size_t                                   size = 100;
    std::vector<std::unique_ptr<virtual_call::Entity>> entities;

    entities.reserve(size);

    for (size_t i = 0; i < size; ++i) {
        if (i % 2) {
            entities.push_back(std::make_unique<virtual_call::Human<true>>());
        }
        else {
            entities.push_back(std::make_unique<virtual_call::Car<true>>());
        }
    }

    for (auto _ : state) {
        for (size_t i = 0; i < 10000; ++i) {
            for (auto& e : entities) {
                e->tick();
            }
        }
    }
}

void
BENCHMARK_VirtualCall(benchmark::State& state)
{
    constexpr size_t                                   size = 100;
    std::vector<std::unique_ptr<virtual_call::Entity>> entities;

    entities.reserve(size);

    for (size_t i = 0; i < size; ++i) {
        if (i % 2) {
            entities.push_back(std::make_unique<virtual_call::Human<false>>());
        }
        else {
            entities.push_back(std::make_unique<virtual_call::Car<false>>());
        }
    }

    for (auto _ : state) {
        for (size_t i = 0; i < 10000; ++i) {
            for (auto& e : entities) {
                e->tick();
            }
        }
    }
}

void
BENCHMARK_CRTPInline(benchmark::State& state)
{
    constexpr size_t               size = 50;
    std::vector<crtp::Human<true>> humans;
    std::vector<crtp::Car<true>>   cars;

    humans.reserve(size);
    cars.reserve(size);

    for (size_t i = 0; i < size; ++i) {
        humans.emplace_back();
        cars.emplace_back();
    }

    for (auto _ : state) {
        for (size_t i = 0; i < 10000; ++i) {
            for (auto& e : humans) {
                e.tick();
            }
            for (auto& e : cars) {
                e.tick();
            }
        }
    }
}

void
BENCHMARK_CRTP(benchmark::State& state)
{
    constexpr size_t                size = 50;
    std::vector<crtp::Human<false>> humans;
    std::vector<crtp::Car<false>>   cars;

    humans.reserve(size);
    cars.reserve(size);

    for (size_t i = 0; i < size; ++i) {
        humans.emplace_back();
        cars.emplace_back();
    }

    for (auto _ : state) {
        for (size_t i = 0; i < 10000; ++i) {
            for (auto& e : humans) {
                e.tick();
            }
            for (auto& e : cars) {
                e.tick();
            }
        }
    }
}

void
BENCHMARK_VariantInline(benchmark::State& state)
{
    using Entity          = std::variant<variant::Human<true>, variant::Car<true>>;
    constexpr size_t size = 100;

    std::vector<Entity> entities(size);

    entities.reserve(size);

    for (size_t i = 0; i < size; ++i) {
        if (i % 2) {
            entities[i].emplace<variant::Human<true>>();
        }
        else {
            entities[i].emplace<variant::Car<true>>();
        }
    }

    variant::overload o{[](variant::Human<true>& h) {
                            h.tick();
                        },
                        [](variant::Car<true>& c) {
                            c.tick();
                        }};

    for (auto _ : state) {
        for (size_t i = 0; i < 10000; ++i) {
            for (auto& e : entities) {
                std::visit(o, e);
            }
        }
    }
}

void
BENCHMARK_Variant(benchmark::State& state)
{
    using Entity          = std::variant<variant::Human<false>, variant::Car<false>>;
    constexpr size_t size = 100;

    std::vector<Entity> entities(size);

    entities.reserve(size);

    for (size_t i = 0; i < size; ++i) {
        if (i % 2) {
            entities[i].emplace<variant::Human<false>>();
        }
        else {
            entities[i].emplace<variant::Car<false>>();
        }
    }

    variant::overload o{[](variant::Human<false>& h) {
                            h.tick();
                        },
                        [](variant::Car<false>& c) {
                            c.tick();
                        }};

    for (auto _ : state) {
        for (size_t i = 0; i < 10000; ++i) {
            for (auto& e : entities) {
                std::visit(o, e);
            }
        }
    }
}

void
BENCHMARK_BoostPolyCollectionBaseInline(benchmark::State& state)
{
    constexpr size_t                             size = 100;
    boost::base_collection<virtual_call::Entity> entities;

    entities.reserve(size);

    for (size_t i = 0; i < size; ++i) {
        if (i % 2) {
            entities.insert(virtual_call::Human<true>{});
        }
        else {
            entities.insert(virtual_call::Car<true>{});
        }
    }

    for (auto _ : state) {
        for (size_t i = 0; i < 10000; ++i) {
            for (auto& e : entities) {
                e.tick();
            }
        }
    }
}

void
BENCHMARK_BoostPolyCollectionBase(benchmark::State& state)
{
    constexpr size_t                             size = 100;
    boost::base_collection<virtual_call::Entity> entities;

    entities.reserve(size);

    for (size_t i = 0; i < size; ++i) {
        if (i % 2) {
            entities.insert(virtual_call::Human<false>{});
        }
        else {
            entities.insert(virtual_call::Car<false>{});
        }
    }

    for (auto _ : state) {
        for (size_t i = 0; i < 10000; ++i) {
            for (auto& e : entities) {
                e.tick();
            }
        }
    }
}

void
BENCHMARK_BoostPolyCollectionVariantInline(benchmark::State& state)
{
    constexpr size_t                                                       size = 100;
    boost::variant_collection_of<variant::Human<true>, variant::Car<true>> entities;

    entities.reserve(size);

    for (size_t i = 0; i < size; ++i) {
        if (i % 2) {
            entities.insert(variant::Human<true>{});
        }
        else {
            entities.insert(variant::Car<true>{});
        }
    }

    variant::overload o{[](variant::Human<true>& h) {
                            h.tick();
                        },
                        [](variant::Car<true>& c) {
                            c.tick();
                        }};

    for (auto _ : state) {
        for (size_t i = 0; i < 10000; ++i) {
            for (auto& e : entities) {
                boost::poly_collection::visit(o, e);
            }
        }
    }
}

void
BENCHMARK_BoostPolyCollectionVariant(benchmark::State& state)
{
    constexpr size_t                                                         size = 100;
    boost::variant_collection_of<variant::Human<false>, variant::Car<false>> entities;

    entities.reserve(size);

    for (size_t i = 0; i < size; ++i) {
        if (i % 2) {
            entities.insert(variant::Human<false>{});
        }
        else {
            entities.insert(variant::Car<false>{});
        }
    }

    variant::overload o{[](variant::Human<false>& h) {
                            h.tick();
                        },
                        [](variant::Car<false>& c) {
                            c.tick();
                        }};

    for (auto _ : state) {
        for (size_t i = 0; i < 10000; ++i) {
            for (auto& e : entities) {
                boost::poly_collection::visit(o, e);
            }
        }
    }
}

void
BENCHMARK_MoveOnlyFunctionInline(benchmark::State& state)
{
    constexpr size_t                             size = 100;
    std::vector<std::move_only_function<void()>> entities;

    entities.reserve(size);

    for (size_t i = 0; i < size; ++i) {
        if (i % 2) {
            entities.push_back([h = variant::Human<true>{}]() mutable {
                h.tick();
            });
        }
        else {
            entities.push_back([c = variant::Car<true>{}]() mutable {
                c.tick();
            });
        }
    }

    for (auto _ : state) {
        for (size_t i = 0; i < 10000; ++i) {
            for (auto& e : entities) {
                e();
            }
        }
    }
}

void
BENCHMARK_MoveOnlyFunction(benchmark::State& state)
{
    constexpr size_t                             size = 100;
    std::vector<std::move_only_function<void()>> entities;

    entities.reserve(size);

    for (size_t i = 0; i < size; ++i) {
        if (i % 2) {
            entities.push_back([h = variant::Human<false>{}]() mutable {
                h.tick();
            });
        }
        else {
            entities.push_back([c = variant::Car<false>{}]() mutable {
                c.tick();
            });
        }
    }

    for (auto _ : state) {
        for (size_t i = 0; i < 10000; ++i) {
            for (auto& e : entities) {
                e();
            }
        }
    }
}
}  // namespace

BENCHMARK(BENCHMARK_VirtualCallInline);
BENCHMARK(BENCHMARK_VirtualCall);
BENCHMARK(BENCHMARK_CRTPInline);
BENCHMARK(BENCHMARK_CRTP);
BENCHMARK(BENCHMARK_VariantInline);
BENCHMARK(BENCHMARK_Variant);
BENCHMARK(BENCHMARK_BoostPolyCollectionBaseInline);
BENCHMARK(BENCHMARK_BoostPolyCollectionBase);
BENCHMARK(BENCHMARK_BoostPolyCollectionVariantInline);
BENCHMARK(BENCHMARK_BoostPolyCollectionVariant);
BENCHMARK(BENCHMARK_MoveOnlyFunctionInline);
BENCHMARK(BENCHMARK_MoveOnlyFunction);

BENCHMARK_MAIN();

// int
// main()
// {
//     constexpr size_t                                   size = 100;
//     std::vector<std::unique_ptr<virtual_call::Entity>> entities;

//     entities.reserve(size);

//     for (size_t i = 0; i < size; ++i) {
//         if (i % 2) {
//             entities.push_back(std::make_unique<virtual_call::Human<true>>());
//         }
//         else {
//             entities.push_back(std::make_unique<virtual_call::Car<true>>());
//         }
//     }

//     for (size_t i = 0; i < 10000; ++i) {
//         for (auto& e : entities) {
//             e->tick();
//         }
//     }

//     return 0;
// }