#include "example06/bufferpool.hpp"
#include "example06/ringbuffer.hpp"

#include <benchmark/benchmark.h>
#include <boost/lockfree/spsc_queue.hpp>

#include <iostream>
#include <utility>

namespace
{
struct Element
{
    std::string str_;

    // Element()
    // {
    //     std::cout << "default constructor\n";
    // }

    // explicit Element(std::string str)
    //   : str_{std::move(str)}
    // {
    //     std::cout << "constructor " << str << "\n";
    // }

    // ~Element()
    // {
    //     std::cout << "destructor\n";
    // }

    // Element(const Element& other)
    //   : str_{other.str_}
    // {
    //     std::cout << "copy constructor\n";
    // }

    // Element&
    // operator=(const Element& other)
    // {
    //     if (this != &other) {
    //         str_ = other.str_;
    //     }

    //     std::cout << "copy assigned\n";

    //     return *this;
    // }

    // Element(Element&& other) noexcept
    //   : str_{std::exchange(other.str_, "")}
    // {
    //     std::cout << "move constructor\n";
    // }

    // Element&
    // operator=(Element&& other) noexcept
    // {
    //     str_ = std::exchange(other.str_, "");

    //     std::cout << "move assigned\n";

    //     return *this;
    // }
};
}  // namespace

namespace
{
void
BENCHMARK_RingBuffer(benchmark::State& state)
{
    for (auto _ : state) {
        RingBuffer<Element> ringBuffer(1 << 10);

        std::thread producer{[&ringBuffer]() {
            for (size_t i = 0; i < 1'000'000; ++i) {
                ringBuffer.push(std::to_string(i));
            }
        }};

        std::thread consumer{[&ringBuffer]() {
            Element element;

            for (size_t i = 0; i < 1'000'000; ++i) {
                static_cast<void>(ringBuffer.pop(element));
            }
        }};

        producer.join();
        consumer.join();
    }
}

void
BENCHMARK_SPSCRingBuffer(benchmark::State& state)
{
    for (auto _ : state) {
        SPSCRingBuffer<Element> ringBuffer(1 << 10);

        std::thread producer{[&ringBuffer]() {
            for (size_t i = 0; i < 1'000'000; ++i) {
                ringBuffer.push(std::to_string(i));
            }
        }};

        std::thread consumer{[&ringBuffer]() {
            size_t i = 0;

            while (i < 1'000'000) {
                if (Element* element = ringBuffer.front()) {
                    static_cast<void>(element);
                    ringBuffer.pop();
                    ++i;
                }
            }
        }};

        producer.join();
        consumer.join();
    }
}

void
BENCHMARK_BoostSPSCQueue(benchmark::State& state)
{
    for (auto _ : state) {
        boost::lockfree::spsc_queue<Element> SPSCQueue(1 << 10);

        std::thread producer{[&SPSCQueue]() {
            size_t i = 0;

            while (i < 1'000'000) {
                if (SPSCQueue.push({std::to_string(i)})) {
                    ++i;
                }
            }
        }};

        std::thread consumer{[&SPSCQueue]() {
            Element element;
            size_t  i = 0;

            while (i < 1'000'000) {
                if (SPSCQueue.pop(element)) {
                    ++i;
                }
            }
        }};

        producer.join();
        consumer.join();
    }
}

void
BENCHMARK_MPMCRingBuffer_Single(benchmark::State& state)
{
    for (auto _ : state) {
        MPMCRingBuffer<Element> ringBuffer(1 << 10);

        std::thread producer{[&ringBuffer]() {
            for (size_t i = 0; i < 1'000'000; ++i) {
                ringBuffer.push(std::to_string(i));
            }
        }};

        std::thread consumer{[&ringBuffer]() {
            Element element;
            for (size_t i = 0; i < 1'000'000; ++i) {
                ringBuffer.pop(element);
            }
        }};

        producer.join();
        consumer.join();
    }
}

void
BENCHMARK_MPMCRingBuffer(benchmark::State& state)
{
    for (auto _ : state) {
        MPMCRingBuffer<Element> ringBuffer(1 << 10);

        std::thread producer1{[&ringBuffer]() {
            for (size_t i = 0; i < 500'000; ++i) {
                ringBuffer.push(std::to_string(i));
            }
        }};

        std::thread producer2{[&ringBuffer]() {
            for (size_t i = 0; i < 500'000; ++i) {
                ringBuffer.push(std::to_string(i));
            }
        }};

        std::thread consumer1{[&ringBuffer]() {
            Element element;
            for (size_t i = 0; i < 500'000; ++i) {
                ringBuffer.pop(element);
            }
        }};

        std::thread consumer2{[&ringBuffer]() {
            Element element;
            for (size_t i = 0; i < 500'000; ++i) {
                ringBuffer.pop(element);
            }
        }};

        producer1.join();
        producer2.join();
        consumer1.join();
        consumer2.join();
    }
}
}  // namespace

BENCHMARK(BENCHMARK_RingBuffer);
BENCHMARK(BENCHMARK_BoostSPSCQueue);
BENCHMARK(BENCHMARK_SPSCRingBuffer);
BENCHMARK(BENCHMARK_MPMCRingBuffer_Single);
BENCHMARK(BENCHMARK_MPMCRingBuffer);

BENCHMARK_MAIN();
