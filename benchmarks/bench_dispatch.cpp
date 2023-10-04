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
BENCHMARK_VirtualCall(benchmark::State& state)
{
    constexpr size_t                                   size = 100;
    std::vector<std::unique_ptr<virtual_call::Entity>> entities;

    entities.reserve(size);

    for (size_t i = 0; i < size; ++i) {
        if (i % 2) {
            entities.push_back(std::make_unique<virtual_call::Human>());
        }
        else {
            entities.push_back(std::make_unique<virtual_call::Car>());
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
BENCHMARK_CRTP(benchmark::State& state)
{
    constexpr size_t         size = 50;
    std::vector<crtp::Human> humans;
    std::vector<crtp::Car>   cars;

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
BENCHMARK_Variant(benchmark::State& state)
{
    using Entity          = std::variant<variant::Human, variant::Car>;
    constexpr size_t size = 100;

    std::vector<Entity> entities(size);

    entities.reserve(size);

    for (size_t i = 0; i < size; ++i) {
        if (i % 2) {
            entities[i].emplace<variant::Human>();
        }
        else {
            entities[i].emplace<variant::Car>();
        }
    }

    variant::overload o{[](variant::Human& h) {
                            h.tick();
                        },
                        [](variant::Car& c) {
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
BENCHMARK_BoostPolyCollectionBase(benchmark::State& state)
{
    constexpr size_t                             size = 100;
    boost::base_collection<virtual_call::Entity> entities;

    entities.reserve(size);

    for (size_t i = 0; i < size; ++i) {
        if (i % 2) {
            entities.insert(virtual_call::Human{});
        }
        else {
            entities.insert(virtual_call::Car{});
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
BENCHMARK_BoostPolyCollectionVariant(benchmark::State& state)
{
    constexpr size_t                                           size = 100;
    boost::variant_collection_of<variant::Human, variant::Car> entities;

    entities.reserve(size);

    for (size_t i = 0; i < size; ++i) {
        if (i % 2) {
            entities.insert(variant::Human{});
        }
        else {
            entities.insert(variant::Car{});
        }
    }

    variant::overload o{[](variant::Human& h) {
                            h.tick();
                        },
                        [](variant::Car& c) {
                            c.tick();
                        }};

    for (auto _ : state) {
        for (size_t i = 0; i < 10000; ++i) {
            for (auto& e : entities) {
                boost::poly_collection::visit(o, e);
            }
        }

        // for (size_t i = 0; i < 10000; ++i) {
        //     for (auto& e : entities) {
        //         boost::poly_collection::visit_by_index(
        //             e,
        //             [](variant::Human& h) {
        //                 h.tick();
        //             },
        //             [](variant::Car& c) {
        //                 c.tick();
        //             });
        //     }
        // }
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
            entities.push_back([h = variant::Human{}]() mutable {
                h.tick();
            });
        }
        else {
            entities.push_back([c = variant::Car{}]() mutable {
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

BENCHMARK(BENCHMARK_VirtualCall);
BENCHMARK(BENCHMARK_CRTP);
BENCHMARK(BENCHMARK_Variant);
BENCHMARK(BENCHMARK_BoostPolyCollectionBase);
BENCHMARK(BENCHMARK_BoostPolyCollectionVariant);
BENCHMARK(BENCHMARK_MoveOnlyFunction);

BENCHMARK_MAIN();

// int
// main()
// {
//     using Entity          = std::variant<std::monostate, variant::Human, variant::Car>;
//     constexpr size_t size = 100;

//     std::vector<Entity> entities(size, std::monostate{});

//     entities.reserve(size);

//     for (size_t i = 0; i < size; ++i) {
//         if (i % 2) {
//             entities[i].emplace<variant::Human>();
//         }
//         else {
//             entities[i].emplace<variant::Car>();
//         }
//     }

//     variant::overload o{[](variant::Human& h) {
//                             h.tick();
//                         },
//                         [](variant::Car& c) {
//                             c.tick();
//                         },
//                         [](auto&) {
//                         }};

//     for (size_t i = 0; i < 10000; ++i) {
//         for (auto& e : entities) {
//             std::visit(o, e);
//         }
//     }

//     return 0;
// }