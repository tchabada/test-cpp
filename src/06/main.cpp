#include "ringbuffer.hpp"

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


#include <string>

int
main()
{
    {
        std::cout << "RingBuffer:\n";

        RingBuffer<Element> ringBuffer(16);

        std::thread producer1{[&ringBuffer]() {
            for (size_t i = 0; i < 50; ++i) {
                ringBuffer.push(std::to_string(i));
            }
        }};

        std::thread producer2{[&ringBuffer]() {
            for (size_t i = 50; i < 100; ++i) {
                ringBuffer.push(std::to_string(i));
            }
        }};

        std::thread consumer1{[&ringBuffer]() {
            Element element;

            for (size_t i = 0; i < 50; ++i) {
                if (ringBuffer.pop(element)) {
                    std::cout << element.str_ << '\n';
                }
            }
        }};

        std::thread consumer2{[&ringBuffer]() {
            Element element;

            for (size_t i = 0; i < 50; ++i) {
                if (ringBuffer.pop(element)) {
                    std::cout << element.str_ << '\n';
                }
            }
        }};

        producer1.join();
        producer2.join();
        consumer1.join();
        consumer2.join();
    }

    {
        std::cout << "\n\nSPSCRingBuffer:\n";

        SPSCRingBuffer<Element> ringBuffer(16);

        std::thread procuder{[&ringBuffer]() {
            for (size_t i = 0; i < 100; ++i) {
                ringBuffer.push(std::to_string(i));
            }
        }};

        std::thread consumer{[&ringBuffer]() {
            size_t i = 0;

            while (i < 100) {
                if (Element* element = ringBuffer.front()) {
                    std::cout << element->str_ << '\n';
                    ringBuffer.pop();
                    ++i;
                }
            }
        }};

        procuder.join();
        consumer.join();
    }

    {
        std::cout << "\n\nMPMCRingBuffer:\n";

        MPMCRingBuffer<Element> ringBuffer(16);

        std::thread producer1{[&ringBuffer]() {
            for (size_t i = 0; i < 50; ++i) {
                ringBuffer.push(std::to_string(i));
            }
        }};

        std::thread producer2{[&ringBuffer]() {
            for (size_t i = 50; i < 100; ++i) {
                ringBuffer.push(std::to_string(i));
            }
        }};

        std::thread consumer1{[&ringBuffer]() {
            Element element;

            for (size_t i = 0; i < 50; ++i) {
                ringBuffer.pop(element);
                std::cout << element.str_ << '\n';
            }
        }};

        std::thread consumer2{[&ringBuffer]() {
            Element element;

            for (size_t i = 0; i < 50; ++i) {
                ringBuffer.pop(element);
                std::cout << element.str_ << '\n';
            }
        }};

        producer1.join();
        producer2.join();
        consumer1.join();
        consumer2.join();
    }

    return 0;
}
