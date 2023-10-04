#pragma once

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <thread>
#include <type_traits>

template <typename T>
class RingBuffer
{
public:
    explicit RingBuffer(const size_t capacity)
      : capacity_{capacity}
      , mask_{capacity - 1}
      , data_{static_cast<T*>(std::malloc((sizeof(T) * capacity_)))}
    {
        assert(capacity_ > 0 && (capacity_ & mask_) == 0);
    }

    RingBuffer(const RingBuffer&)            = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;
    RingBuffer(RingBuffer&&)                 = delete;
    RingBuffer& operator=(RingBuffer&&)      = delete;

    ~RingBuffer()
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

            poppedCondition_.wait(lock, [this]() {
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

        poppedCondition_.notify_one();

        return true;
    }

    [[nodiscard]] std::optional<T>
    pop()
    {
        std::optional<T> result;

        {
            std::unique_lock<std::mutex> lock(mutex_);

            pushedCondition_.wait(lock, [this]() {
                return ((head_ - tail_) & mask_) > 0 || !running_;
            });

            if (!running_) [[unlikely]] {
                return result;
            }

            result = std::move(data_[tail_]);

            data_[tail_].~T();

            tail_ = (tail_ + 1) & mask_;
        }

        poppedCondition_.notify_one();

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
        return capacity_ - 1;
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
            poppedCondition_.notify_all();
        }
    }

private:
    const size_t capacity_;
    const size_t mask_;

    std::condition_variable pushedCondition_;
    std::condition_variable poppedCondition_;
    mutable std::mutex      mutex_;

    T* data_;

    size_t head_{0};
    size_t tail_{0};

    std::atomic<bool> running_{true};
};

#ifdef __cpp_lib_hardware_interference_size
static constexpr size_t kCacheLineSize = std::hardware_destructive_interference_size;
#else
static constexpr size_t kCacheLineSize = 64;
#endif

template <typename T>
class alignas(kCacheLineSize) SPSCRingBuffer
{
public:
    explicit SPSCRingBuffer(const size_t capacity)
      : capacity_{capacity}
      , mask_{capacity - 1}
      , data_{static_cast<T*>(std::malloc(sizeof(T) * (capacity + (2 * kPaddCount))))}
    {
        if (capacity_ == 0 || (capacity_ & mask_) != 0) {
            throw std::invalid_argument("Capacity must be a positive power of two");
        }

        if (!data_) {
            throw std::bad_alloc();
        }
    }

    ~SPSCRingBuffer()
    {
        if (!std::is_trivially_destructible_v<T>) {
            while (front()) {
                pop();
            }
        }

        std::free(data_);
    }

    SPSCRingBuffer(const SPSCRingBuffer&)            = delete;
    SPSCRingBuffer& operator=(const SPSCRingBuffer&) = delete;
    SPSCRingBuffer(SPSCRingBuffer&&)                 = delete;
    SPSCRingBuffer& operator=(SPSCRingBuffer&&)      = delete;

    template <typename... Args>
    void
    emplace(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args&&...>)
    {
        static_assert(std::is_constructible_v<T, Args&&...>, "T must be constructible with Args&&...");

        const auto currentHead = head_.load(std::memory_order_relaxed);
        auto       nextHead    = (currentHead + 1) & mask_;

        while (nextHead == cachedTail_) {
            cachedTail_ = tail_.load(std::memory_order_acquire);
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

        if (nextHead == cachedTail_) {
            cachedTail_ = tail_.load(std::memory_order_acquire);

            if (nextHead == cachedTail_) {
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

        if (currentTail == cachedHead_) {
            cachedHead_ = head_.load(std::memory_order_acquire);

            if (cachedHead_ == currentTail) {
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
        return capacity_ - 1;
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

template <typename T>
class alignas(kCacheLineSize) MPMCRingBuffer
{
    static_assert(alignof(T) <= kCacheLineSize, "Alignment of T must not exceed cache line size");

public:
    explicit MPMCRingBuffer(const std::size_t capacity)
      : capacity_{capacity}
      , mask_{capacity - 1}
    {
        if (capacity_ == 0 || (capacity_ & mask_) != 0) {
            throw std::invalid_argument("Capacity must be a positive power of two");
        }

#ifdef _WIN32
        cells_ = static_cast<Cell*>(_aligned_malloc(kCacheLineSize, sizeof(Cell) * capacity_));
#else
        cells_ = static_cast<Cell*>(std::aligned_alloc(kCacheLineSize, sizeof(Cell) * capacity_));
#endif

        if (!cells_) {
            throw std::bad_alloc();
        }

        for (std::size_t i = 0; i < capacity_; ++i) {
            new (&cells_[i].sequence) std::atomic<std::size_t>(i);
        }
    }

    ~MPMCRingBuffer()
    {
        std::size_t       currentTail = tail_.load(std::memory_order_relaxed);
        const std::size_t currentHead = head_.load(std::memory_order_relaxed);
        std::size_t       remaining   = currentHead - currentTail;

        if (remaining > capacity_) {
            remaining = capacity_;
        }

        while (remaining > 0) {
            Cell&             cell = cells_[currentTail & mask_];
            const std::size_t s    = cell.sequence.load(std::memory_order_relaxed);

            if (s == currentTail + 1) {
                cell.storage()->~T();
            }

            ++currentTail;
            --remaining;
        }

        for (std::size_t i = 0; i < capacity_; ++i) {
            cells_[i].sequence.~atomic();
        }

        std::free(cells_);
    }

    MPMCRingBuffer(const MPMCRingBuffer&)            = delete;
    MPMCRingBuffer& operator=(const MPMCRingBuffer&) = delete;
    MPMCRingBuffer(MPMCRingBuffer&&)                 = delete;
    MPMCRingBuffer& operator=(MPMCRingBuffer&&)      = delete;

    template <typename... Args>
    void
    emplace(Args&&... args)
        requires std::is_nothrow_constructible_v<T, Args&&...>
    {
        std::size_t currentHead = head_.load(std::memory_order_relaxed);

        while (true) {
            Cell&             cell = cells_[currentHead & mask_];
            const std::size_t s    = cell.sequence.load(std::memory_order_acquire);
            const auto        diff = static_cast<std::ptrdiff_t>(s - currentHead);

            if (diff == 0) {
                if (head_.compare_exchange_weak(currentHead, currentHead + 1, std::memory_order_relaxed)) {
                    new (&cell.data) T(std::forward<Args>(args)...);
                    cell.sequence.store(currentHead + 1, std::memory_order_release);

                    return;
                }
            }
            else if (diff < 0) {
                std::this_thread::yield();
            }
            else {
                currentHead = head_.load(std::memory_order_relaxed);
            }
        }
    }

    template <typename... Args>
    [[nodiscard]] bool
    try_emplace(Args&&... args)
        requires std::is_nothrow_constructible_v<T, Args&&...>
    {
        std::size_t currentHead = head_.load(std::memory_order_relaxed);

        while (true) {
            Cell&             cell = cells_[currentHead & mask_];
            const std::size_t s    = cell.sequence.load(std::memory_order_acquire);
            const auto        diff = static_cast<std::ptrdiff_t>(s - currentHead);

            if (diff == 0) {
                if (head_.compare_exchange_weak(currentHead, currentHead + 1, std::memory_order_relaxed)) {
                    new (&cell.data) T(std::forward<Args>(args)...);
                    cell.sequence.store(currentHead + 1, std::memory_order_release);

                    return true;
                }
            }
            else if (diff < 0) {
                return false;
            }
            else {
                currentHead = head_.load(std::memory_order_relaxed);
            }
        }
    }

    void
    push(const T& element)
        requires std::is_nothrow_copy_constructible_v<T>
    {
        emplace(element);
    }

    template <typename U>
    void
    push(U&& element)
        requires std::is_nothrow_constructible_v<T, U&&>
    {
        emplace(std::forward<U>(element));
    }

    [[nodiscard]] bool
    try_push(const T& element)
        requires std::is_nothrow_copy_constructible_v<T>
    {
        return try_emplace(element);
    }

    template <typename U>
    [[nodiscard]] bool
    try_push(U&& element)
        requires std::is_nothrow_constructible_v<T, U&&>
    {
        return try_emplace(std::forward<U>(element));
    }

    void
    pop(T& result)
        requires std::is_nothrow_destructible_v<T> && std::is_nothrow_move_assignable_v<T>
    {
        std::size_t currentTail = tail_.load(std::memory_order_relaxed);

        while (true) {
            Cell&             cell = cells_[currentTail & mask_];
            const std::size_t s    = cell.sequence.load(std::memory_order_acquire);
            const auto        diff = static_cast<std::ptrdiff_t>(s - (currentTail + 1));

            if (diff == 0) {
                if (tail_.compare_exchange_weak(currentTail, currentTail + 1, std::memory_order_relaxed)) {
                    T* ptr = cell.storage();
                    result = std::move(*ptr);
                    ptr->~T();
                    cell.sequence.store(currentTail + mask_ + 1, std::memory_order_release);

                    return;
                }
            }
            else if (diff < 0) {
                std::this_thread::yield();
            }
            else {
                currentTail = tail_.load(std::memory_order_relaxed);
            }
        }
    }

    [[nodiscard]] bool
    try_pop(T& result)
        requires std::is_nothrow_destructible_v<T> && std::is_nothrow_move_assignable_v<T>
    {
        std::size_t currentTail = tail_.load(std::memory_order_relaxed);

        while (true) {
            Cell&             cell = cells_[currentTail & mask_];
            const std::size_t s    = cell.sequence.load(std::memory_order_acquire);
            const auto        diff = static_cast<std::ptrdiff_t>(s - (currentTail + 1));

            if (diff == 0) {
                if (tail_.compare_exchange_weak(currentTail, currentTail + 1, std::memory_order_relaxed)) {
                    T* ptr = cell.storage();
                    result = std::move(*ptr);
                    ptr->~T();
                    cell.sequence.store(currentTail + mask_ + 1, std::memory_order_release);

                    return true;
                }
            }
            else if (diff < 0) {
                return false;
            }
            else {
                currentTail = tail_.load(std::memory_order_relaxed);
            }
        }
    }

    [[nodiscard]] std::optional<T>
    try_pop()
        requires std::is_nothrow_destructible_v<T> && std::is_nothrow_move_constructible_v<T>
    {
        std::size_t currentTail = tail_.load(std::memory_order_relaxed);

        while (true) {
            Cell&             cell = cells_[currentTail & mask_];
            const std::size_t s    = cell.sequence.load(std::memory_order_acquire);
            const auto        diff = static_cast<std::ptrdiff_t>(s - (currentTail + 1));

            if (diff == 0) {
                if (tail_.compare_exchange_weak(currentTail, currentTail + 1, std::memory_order_relaxed)) {
                    T*               ptr = cell.storage();
                    std::optional<T> result{std::move(*ptr)};
                    ptr->~T();
                    cell.sequence.store(currentTail + mask_ + 1, std::memory_order_release);

                    return result;
                }
            }
            else if (diff < 0) {
                return std::nullopt;
            }
            else {
                currentTail = tail_.load(std::memory_order_relaxed);
            }
        }
    }

    [[nodiscard]] std::size_t
    size() const noexcept
    {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        const std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t diff = head - tail;

        if (diff > capacity_) {
            return 0;
        }

        return diff;
    }

    [[nodiscard]] bool
    empty() const noexcept
    {
        return size() == 0;
    }

    [[nodiscard]] std::size_t
    capacity() const noexcept
    {
        return capacity_;
    }

private:
    struct alignas(kCacheLineSize) Cell
    {
        std::atomic<std::size_t> sequence;
        alignas(T) std::byte data[sizeof(T)];

        [[nodiscard]] T*
        storage() noexcept
        {
            return std::launder(reinterpret_cast<T*>(&data));
        }

        [[nodiscard]] const T*
        storage() const noexcept
        {
            return std::launder(reinterpret_cast<const T*>(&data));
        }
    };

    const std::size_t capacity_;
    const std::size_t mask_;

    Cell* cells_;

    alignas(kCacheLineSize) std::atomic<std::size_t> head_{0};
    alignas(kCacheLineSize) std::atomic<std::size_t> tail_{0};
};
