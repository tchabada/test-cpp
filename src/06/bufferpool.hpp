#pragma once

#include <array>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>

template <typename T, std::size_t N>
class BufferPool
{
    static_assert(N > 0, "N must be greater than 0");

public:
    BufferPool()
    {
        for (std::size_t i = 0; i < N; ++i) {
            release(reinterpret_cast<T*>(memory_.data() + (kElementSize * i)));
        }
    }

    BufferPool(const BufferPool&)            = delete;
    BufferPool& operator=(const BufferPool&) = delete;

    T*
    acquire()
    {
        if (void* free = head_) {
            head_ = *reinterpret_cast<void**>(free);
            return reinterpret_cast<T*>(free);

            // std::memcpy(reinterpret_cast<void*>(&head_), free, sizeof(head_));
            // return reinterpret_cast<T*>(free);
        }

        return nullptr;
    }

    void
    release(T* p)
    {
        *reinterpret_cast<void**>(p) = head_;
        head_                        = p;

        // head_ = memcpy(reinterpret_cast<void*>(p), reinterpret_cast<void*>(&head_), sizeof(head_));
    }

private:
    static constexpr std::size_t kElementAlign   = std::max(alignof(T), alignof(void*));
    static constexpr std::size_t kRawElementSize = std::max(sizeof(T), sizeof(void*));
    static constexpr std::size_t kElementSize = ((kRawElementSize + kElementAlign - 1) / kElementAlign) * kElementAlign;

    alignas(kElementAlign) std::array<uint8_t, N * kElementSize> memory_{};

    void* head_{nullptr};
};
