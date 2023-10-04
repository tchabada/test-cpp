#include "ringbuffer.hpp"

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

#include <benchmark/benchmark.h>

namespace
{
void
BENCHMARK_RingBufferCV(benchmark::State& state)
{
    for (auto _ : state) {
        RingBufferCV<Element> ringBuffer(1 << 10);

        std::thread producer{[&ringBuffer]() {
            for (size_t i = 0; i < 10'000'000; ++i) {
                ringBuffer.push(std::to_string(i));
            }
        }};

        std::thread consumer{[&ringBuffer]() {
            Element element;

            for (size_t i = 0; i < 10'000'000; ++i) {
                static_cast<void>(ringBuffer.pop(element));
            }
        }};

        producer.join();
        consumer.join();
    }
}

void
BENCHMARK_RingBuffeAtomic(benchmark::State& state)
{
    for (auto _ : state) {
        RingBufferAtomic<Element> ringBuffer(1 << 10);

        std::thread producer{[&ringBuffer]() {
            for (size_t i = 0; i < 10'000'000; ++i) {
                ringBuffer.push(std::to_string(i));
            }
        }};

        std::thread consumer{[&ringBuffer]() {
            size_t i = 0;

            while (i < 10'000'000) {
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
            size_t  i = 0;

            while (i < 10'000'000) {
                if (SPSCQueue.push({std::to_string(i)})) {
                    ++i;
                }
            }
        }};

        std::thread consumer{[&SPSCQueue]() {
            Element element;
            size_t  i = 0;

            while (i < 10'000'000) {
                if (SPSCQueue.pop(element)) {
                    ++i;
                }
            }
        }};

        producer.join();
        consumer.join();
    }
}
}  // namespace

BENCHMARK(BENCHMARK_RingBufferCV);
BENCHMARK(BENCHMARK_RingBuffeAtomic);
BENCHMARK(BENCHMARK_BoostSPSCQueue);

BENCHMARK_MAIN();

// #include <string>

// int
// main()
// {
//     {
//         std::cout << "RingBufferCV:\n\n";

//         RingBufferCV<Element> ringBufferCV(16);

//         std::thread producer1{[&ringBufferCV]() {
//             for (size_t i = 0; i < 50; ++i) {
//                 ringBufferCV.push(std::to_string(i));
//             }
//         }};

//         std::thread producer2{[&ringBufferCV]() {
//             for (size_t i = 50; i < 100; ++i) {
//                 ringBufferCV.push(std::to_string(i));
//             }
//         }};

//         std::thread consumer1{[&ringBufferCV]() {
//             Element element;

//             for (size_t i = 0; i < 50; ++i) {
//                 if (ringBufferCV.pop(element)) {
//                     std::cout << element.str_ << '\n';
//                 }
//             }
//         }};

//         std::thread consumer2{[&ringBufferCV]() {
//             Element element;

//             for (size_t i = 0; i < 50; ++i) {
//                 if (ringBufferCV.pop(element)) {
//                     std::cout << element.str_ << '\n';
//                 }
//             }
//         }};

//         producer1.join();
//         producer2.join();
//         consumer1.join();
//         consumer2.join();
//     }

//     {
//         std::cout << "\n\nRingBufferAtomic:\n\n";

//         RingBufferAtomic<Element> ringBufferAtomic(16);

//         std::thread procuder{[&ringBufferAtomic]() {
//             for (size_t i = 0; i < 100; ++i) {
//                 ringBufferAtomic.push(std::to_string(i));
//             }
//         }};

//         std::thread consumer{[&ringBufferAtomic]() {
//             size_t i = 0;

//             while (i < 100) {
//                 if (Element* element = ringBufferAtomic.front()) {
//                     std::cout << element->str_ << '\n';
//                     ringBufferAtomic.pop();
//                     ++i;
//                 }
//             }
//         }};

//         procuder.join();
//         consumer.join();
//     }

//     return 0;
// }
