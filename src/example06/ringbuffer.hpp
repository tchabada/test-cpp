#pragma once

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <optional>
#include <thread>
#include <type_traits>

template <typename T>
class RingBufferCV
{
public:
    explicit RingBufferCV(const size_t capacity)
      : capacity_{capacity}
      , mask_{capacity - 1}
      , data_{static_cast<T*>(std::malloc((sizeof(T) * capacity_)))}
    {
        assert(capacity_ > 0 && (capacity_ & mask_) == 0);
    }

    RingBufferCV(const RingBufferCV&)            = delete;
    RingBufferCV& operator=(const RingBufferCV&) = delete;
    RingBufferCV(RingBufferCV&&)                 = delete;
    RingBufferCV& operator=(RingBufferCV&&)      = delete;

    ~RingBufferCV()
    {
        if (!std::is_trivially_destructible_v<T>) {
            while (tail_ != head_) {
                data_[tail_].~T();
                tail_ = (tail_ + 1) & mask_;
            }
        }

        std::free(data_);
    }

    template <typename... Args>
    void
    emplace(Args&&... args)
    {
        static_assert(std::is_constructible_v<T, Args&&...>, "T must be constructible with Args&&...");

        {
            std::unique_lock<std::mutex> lock{mutex_};

            popedCondition_.wait(lock, [this]() {
                return ((head_ - tail_) & mask_) < capacity_ - 1 || !running_;
            });

            if (!running_) [[unlikely]] {
                return;
            }

            new (&data_[head_]) T(std::forward<Args>(args)...);

            head_ = (head_ + 1) & mask_;
        }

        pushedCondition_.notify_one();
    }

    void
    push(const T& element)
    {
        static_assert(std::is_copy_constructible_v<T>, "T must be copy constructible");

        emplace(element);
    }

    template <typename U>
    void
    push(U&& element)
        requires std::is_constructible_v<T, U&&>
    {
        emplace(std::forward<U>(element));
    }

    [[nodiscard]] bool
    pop(T& result)
    {
        {
            std::unique_lock<std::mutex> lock(mutex_);

            pushedCondition_.wait(lock, [this]() {
                return ((head_ - tail_) & mask_) > 0 || !running_;
            });

            if (!running_) [[unlikely]] {
                return false;
            }

            result = std::move(data_[tail_]);

            data_[tail_].~T();

            tail_ = (tail_ + 1) & mask_;
        }

        popedCondition_.notify_one();

        return true;
    }

    [[nodiscard]] std::optional<T>
    pop()
    {
        std::unique_lock<std::mutex> lock(mutex_);

        pushedCondition_.wait(lock, [this]() {
            return ((head_ - tail_) & mask_) > 0 || !running_;
        });

        if (!running_) [[unlikely]] {
            return {};
        }

        std::optional<T> result{std::move(data_[tail_])};

        data_[tail_].~T();

        tail_ = (tail_ + 1) & mask_;

        popedCondition_.notify_one();

        return result;
    }

    [[nodiscard]] size_t
    size() const
    {
        const std::lock_guard<std::mutex> lock(mutex_);

        return (head_ - tail_) & mask_;
    }

    [[nodiscard]] size_t
    capacity() const noexcept
    {
        return capacity_;
    }

    void
    stop()
    {
        bool expected = true;

        if (running_.compare_exchange_weak(expected, false)) {
            {
                std::lock_guard<std::mutex> lock{mutex_};
            }

            pushedCondition_.notify_all();
            popedCondition_.notify_all();
        }
    }

private:
    const size_t capacity_;
    const size_t mask_;

    std::condition_variable pushedCondition_;
    std::condition_variable popedCondition_;
    mutable std::mutex      mutex_;

    T* data_;

    size_t head_{0};
    size_t tail_{0};

    std::atomic<bool> running_{true};
};

inline static constexpr size_t kCacheLineSize = std::hardware_destructive_interference_size;

template <typename T>
class alignas(kCacheLineSize) RingBufferAtomic
{
public:
    explicit RingBufferAtomic(const size_t capacity)
      : capacity_{capacity}
      , mask_{capacity - 1}
      , data_{static_cast<T*>(std::malloc(sizeof(T) * (capacity + (2 * kPaddCount))))}
    {
        assert(capacity_ > 0 && (capacity_ & mask_) == 0);
    }

    ~RingBufferAtomic()
    {
        while (front()) {
            pop();
        }

        std::free(data_);
    }

    RingBufferAtomic(const RingBufferAtomic&)            = delete;
    RingBufferAtomic& operator=(const RingBufferAtomic&) = delete;
    RingBufferAtomic(RingBufferAtomic&&)                 = delete;
    RingBufferAtomic& operator=(RingBufferAtomic&&)      = delete;

    template <typename... Args>
    void
    emplace(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args&&...>)
    {
        static_assert(std::is_constructible_v<T, Args&&...>, "T must be constructible with Args&&...");

        const auto currentHead = head_.load(std::memory_order_relaxed);
        auto       nextHead    = (currentHead + 1) & mask_;

        while (nextHead == cachedHead_) {
            cachedHead_ = tail_.load(std::memory_order_acquire);
        }

        new (&data_[currentHead + kPaddCount]) T(std::forward<Args>(args)...);

        head_.store(nextHead, std::memory_order_release);
    }

    template <typename... Args>
    [[nodiscard]] bool
    try_emplace(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args&&...>)
    {
        static_assert(std::is_constructible_v<T, Args&&...>, "T must be constructible with Args&&...");

        const auto currentHead = head_.load(std::memory_order_relaxed);
        auto       nextHead    = (currentHead + 1) & mask_;

        if (nextHead == cachedHead_) {
            cachedHead_ = tail_.load(std::memory_order_acquire);

            if (nextHead == cachedHead_) {
                return false;
            }
        }

        new (&data_[currentHead + kPaddCount]) T(std::forward<Args>(args)...);

        head_.store(nextHead, std::memory_order_release);

        return true;
    }

    void
    push(const T& element) noexcept(std::is_nothrow_copy_constructible_v<T>)
    {
        static_assert(std::is_copy_constructible_v<T>, "T must be copy constructible");

        emplace(element);
    }

    template <typename U>
    void
    push(U&& element) noexcept(std::is_nothrow_constructible_v<T, U&&>)
        requires std::is_constructible_v<T, U&&>
    {
        emplace(std::forward<U>(element));
    }

    [[nodiscard]] bool
    try_push(const T& element) noexcept(std::is_nothrow_copy_constructible_v<T>)
    {
        static_assert(std::is_copy_constructible_v<T>, "T must be copy constructible");

        return try_emplace(element);
    }

    template <typename U>
    [[nodiscard]] bool
    try_push(U&& element) noexcept(std::is_nothrow_constructible_v<T, U&&>)
        requires std::is_constructible_v<T, U&&>
    {
        return try_emplace(std::forward<U>(element));
    }

    [[nodiscard]] T*
    front() noexcept
    {
        const auto currentTail = tail_.load(std::memory_order_relaxed);

        if (currentTail == cachedTail_) {
            cachedTail_ = head_.load(std::memory_order_acquire);

            if (cachedTail_ == currentTail) {
                return nullptr;
            }
        }

        return &data_[currentTail + kPaddCount];
    }

    void
    pop() noexcept
    {
        static_assert(std::is_nothrow_destructible_v<T>, "T must be nothrow destructible");

        const auto currentTail = tail_.load(std::memory_order_relaxed);

        assert(head_.load(std::memory_order_acquire) != currentTail && "Empty");

        data_[currentTail + kPaddCount].~T();

        auto nextTail = (currentTail + 1) & mask_;

        tail_.store(nextTail, std::memory_order_release);
    }

    [[nodiscard]] size_t
    size() const noexcept
    {
        const size_t currentHead = head_.load(std::memory_order_acquire);
        const size_t currentTail = tail_.load(std::memory_order_acquire);

        return (currentHead - currentTail) & mask_;
    }

    [[nodiscard]] bool
    empty() const noexcept
    {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }

    [[nodiscard]] size_t
    capacity() const noexcept
    {
        return capacity_;
    }

private:
    static constexpr size_t kPaddCount = ((kCacheLineSize - 1) / sizeof(T)) + 1;

private:
    const size_t capacity_;
    const size_t mask_;

    T* data_;

    alignas(kCacheLineSize) std::atomic<size_t> head_{0};
    alignas(kCacheLineSize) size_t cachedHead_{0};
    alignas(kCacheLineSize) std::atomic<size_t> tail_{0};
    alignas(kCacheLineSize) size_t cachedTail_{0};
};
