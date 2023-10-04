#pragma once

#include <array>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

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
    static constexpr std::size_t kElementSize = std::max(sizeof(T), sizeof(void*));

    alignas(std::max(alignof(T), alignof(void*))) std::array<uint8_t, N * kElementSize> memory_{};

    void* head_{nullptr};
};

template <typename T, std::size_t N>
class LockFreeBufferPool
{
    static_assert(N > 0, "N must be greater than 0");
    static_assert(N <= UINT32_MAX, "N must fit in uint32_t");

    static constexpr std::size_t kElementSize  = std::max(sizeof(T), sizeof(uint32_t));
    static constexpr std::size_t kElementAlign = std::max(alignof(T), alignof(uint32_t));

    static constexpr uint32_t kNull = UINT32_MAX;  // sentinel: "no next slot"

    struct TaggedIndex
    {
        uint32_t index;
        uint32_t tag;
    };

    static_assert(sizeof(TaggedIndex) == sizeof(uint64_t));

public:
    LockFreeBufferPool()
    {
        for (std::size_t i = 0; i < N; ++i) {
            next_of(static_cast<uint32_t>(i)) = (i + 1 < N) ? static_cast<uint32_t>(i + 1) : kNull;
        }

        head_.store(TaggedIndex{0, 0}, std::memory_order_relaxed);
    }

    LockFreeBufferPool(const LockFreeBufferPool&)            = delete;
    LockFreeBufferPool& operator=(const LockFreeBufferPool&) = delete;

    T*
    acquire()
    {
        TaggedIndex old_head = head_.load(std::memory_order_acquire);

        while (true) {
            if (old_head.index == kNull) {
                return nullptr;
            }

            TaggedIndex new_head;
            new_head.index = next_of(old_head.index);
            new_head.tag   = old_head.tag + 1;

            if (head_.compare_exchange_weak(old_head, new_head, std::memory_order_acq_rel, std::memory_order_acquire)) {
                return slot_ptr(old_head.index);
            }
        }
    }

    void
    release(T* p)
    {
        const auto idx = index_of(p);

        TaggedIndex old_head = head_.load(std::memory_order_acquire);

        while (true) {
            next_of(idx) = old_head.index;

            TaggedIndex new_head;
            new_head.index = idx;
            new_head.tag   = old_head.tag + 1;

            if (head_.compare_exchange_weak(old_head, new_head, std::memory_order_acq_rel, std::memory_order_acquire)) {
                return;
            }
        }
    }

private:
    T*
    slot_ptr(uint32_t idx)
    {
        return reinterpret_cast<T*>(memory_.data() + (kElementSize * idx));
    }

    uint32_t&
    next_of(uint32_t idx)
    {
        return *reinterpret_cast<uint32_t*>(memory_.data() + (kElementSize * idx));
    }

    uint32_t
    index_of(const T* p) const
    {
        const auto offset = reinterpret_cast<const uint8_t*>(p) - memory_.data();
        assert(offset >= 0);
        assert(static_cast<std::size_t>(offset) % kElementSize == 0);

        const auto idx = static_cast<uint32_t>(static_cast<std::size_t>(offset) / kElementSize);
        assert(idx < N);
        return idx;
    }

    alignas(kElementAlign) std::array<uint8_t, N * kElementSize> memory_{};

    static constexpr std::size_t kCacheLineSize = 64;
    alignas(kCacheLineSize) std::atomic<TaggedIndex> head_;
};
