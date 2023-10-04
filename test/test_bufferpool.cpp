#include <gtest/gtest.h>

#include "example06/bufferpool.hpp"

#include <algorithm>
#include <atomic>
#include <barrier>
#include <set>
#include <thread>
#include <vector>

namespace
{

// A trivial payload that is large enough for the pool's intrusive next-pointer
// and carries a canary to detect memory corruption.
struct Payload
{
    uint64_t canary{0};
    uint64_t value{0};

    static constexpr uint64_t kMagic = 0xDEAD'BEEF'CAFE'BABEull;
};

// ---------------------------------------------------------------------------
// 1. Basic single-threaded sanity
// ---------------------------------------------------------------------------

TEST(BufferPoolTest, SingleThreadAcquireRelease)
{
    constexpr std::size_t kPoolSize = 8;
    LockFreeBufferPool<Payload, kPoolSize>  pool;

    std::vector<Payload*> ptrs;
    ptrs.reserve(kPoolSize);

    // Drain the pool.
    for (std::size_t i = 0; i < kPoolSize; ++i) {
        Payload* p = pool.acquire();
        ASSERT_NE(p, nullptr) << "acquire() returned nullptr on element " << i;
        p->canary = Payload::kMagic;
        p->value  = i;
        ptrs.push_back(p);
    }

    // Pool should be exhausted.
    EXPECT_EQ(pool.acquire(), nullptr);

    // Release all and re-acquire — should get the same set of addresses.
    for (auto* p : ptrs) {
        EXPECT_EQ(p->canary, Payload::kMagic);
        pool.release(p);
    }

    std::set<Payload*> first_set(ptrs.begin(), ptrs.end());
    std::set<Payload*> second_set;

    for (std::size_t i = 0; i < kPoolSize; ++i) {
        Payload* p = pool.acquire();
        ASSERT_NE(p, nullptr);
        second_set.insert(p);
    }

    EXPECT_EQ(first_set, second_set);
    EXPECT_EQ(pool.acquire(), nullptr);

    for (auto* p : second_set) {
        pool.release(p);
    }
}

// ---------------------------------------------------------------------------
// 2. Concurrent acquire/release stress test
//    Many threads repeatedly acquire and release buffers. If the free-list
//    becomes corrupted (e.g. a cycle, lost node, or double-hand-out) we
//    will see duplicate pointers, nullptr where we shouldn't, or a canary
//    violation.
// ---------------------------------------------------------------------------

TEST(BufferPoolTest, ConcurrentStress)
{
    constexpr std::size_t kPoolSize   = 64;
    constexpr std::size_t kNumThreads = 8;
    constexpr std::size_t kIterations = 200'000;

    LockFreeBufferPool<Payload, kPoolSize> pool;

    std::atomic<uint64_t> total_acquires{0};
    std::atomic<uint64_t> total_releases{0};
    std::atomic<bool>     corruption_detected{false};

    std::barrier sync_point(static_cast<std::ptrdiff_t>(kNumThreads));

    auto worker = [&](std::size_t thread_id) {
        sync_point.arrive_and_wait();  // start all threads simultaneously

        std::vector<Payload*> held;
        held.reserve(kPoolSize);

        for (std::size_t i = 0; i < kIterations; ++i) {
            // Try to acquire.
            if (Payload* p = pool.acquire()) {
                // Write a unique canary so we can detect stomped memory.
                p->canary = Payload::kMagic;
                p->value  = thread_id * kIterations + i;
                held.push_back(p);
                total_acquires.fetch_add(1, std::memory_order_relaxed);
            }

            // Release roughly half of what we hold to keep contention high.
            if (held.size() > 2 && (i % 3 == 0)) {
                Payload* p = held.back();
                held.pop_back();

                if (p->canary != Payload::kMagic) {
                    corruption_detected.store(true, std::memory_order_relaxed);
                }

                pool.release(p);
                total_releases.fetch_add(1, std::memory_order_relaxed);
            }
        }

        // Release everything still held.
        for (auto* p : held) {
            if (p->canary != Payload::kMagic) {
                corruption_detected.store(true, std::memory_order_relaxed);
            }
            pool.release(p);
            total_releases.fetch_add(1, std::memory_order_relaxed);
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(kNumThreads);

    for (std::size_t t = 0; t < kNumThreads; ++t) {
        threads.emplace_back(worker, t);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_FALSE(corruption_detected.load());
    EXPECT_EQ(total_acquires.load(), total_releases.load());

    // The pool should be fully drained-and-returned. Verify we can acquire
    // exactly kPoolSize elements.
    std::size_t count = 0;

    while (pool.acquire() != nullptr) {
        ++count;
    }

    EXPECT_EQ(count, kPoolSize);
}

// ---------------------------------------------------------------------------
// 3. ABA-scenario provocation
//
//    The classic ABA problem on a lock-free free-list:
//
//      Thread 1                        Thread 2
//      --------                        --------
//      head = A → B → C               .
//      read head (sees A, next = B)    .
//      <preempted>                     .
//      .                               acquire() → gets A
//      .                               acquire() → gets B
//      .                               release(A) → head = A → C
//      <resumes>                       .
//      CAS(head, A→B, ...) succeeds    ← WRONG! B was already handed out
//
//    Without tagging, the CAS would succeed because head == A again.
//    With our tagged pointer the CAS sees a different tag and retries.
//
//    This test creates maximal contention on a tiny pool to provoke ABA.
// ---------------------------------------------------------------------------

TEST(BufferPoolTest, ABAProvocation)
{
    // A tiny pool maximises the probability that the same slot index
    // re-appears in the head position while another thread is mid-CAS.
    constexpr std::size_t kPoolSize   = 4;
    constexpr std::size_t kNumThreads = 8;
    constexpr std::size_t kIterations = 500'000;

    LockFreeBufferPool<Payload, kPoolSize> pool;

    std::atomic<bool> corruption_detected{false};

    // Track which slots are currently "owned" by a thread.
    // If two threads ever see themselves owning the same slot, ABA happened.
    std::array<std::atomic<int>, kPoolSize> slot_owners{};
    for (auto& s : slot_owners) {
        s.store(-1, std::memory_order_relaxed);
    }

    // We need a way to map a Payload* to its slot index.
    // Acquire one pointer so we can compute the base address.
    Payload* base_ptr = pool.acquire();
    pool.release(base_ptr);

    auto slot_index = [base_ptr](const Payload* p) -> std::size_t {
        constexpr auto kSlotSize = std::max(sizeof(Payload), sizeof(uint32_t));
        auto           offset    = reinterpret_cast<const uint8_t*>(p) - reinterpret_cast<const uint8_t*>(base_ptr);
        return static_cast<std::size_t>(offset) / kSlotSize;
    };

    std::barrier sync_point(static_cast<std::ptrdiff_t>(kNumThreads));

    auto worker = [&](int thread_id) {
        sync_point.arrive_and_wait();

        std::vector<Payload*> held;
        held.reserve(kPoolSize);

        for (std::size_t i = 0; i < kIterations && !corruption_detected.load(std::memory_order_relaxed); ++i) {
            // Acquire as many as we can, immediately.
            if (Payload* p = pool.acquire()) {
                auto idx = slot_index(p);

                // Claim ownership: if someone else already owns it → ABA!
                int expected = -1;
                if (!slot_owners[idx].compare_exchange_strong(expected, thread_id, std::memory_order_acq_rel)) {
                    corruption_detected.store(true, std::memory_order_relaxed);
                    FAIL() << "ABA detected! Slot " << idx << " handed to thread " << thread_id
                           << " while already owned by thread " << expected;
                }

                p->canary = Payload::kMagic;
                p->value  = static_cast<uint64_t>(thread_id);
                held.push_back(p);
            }

            // Release one to create the A→B→A pattern.
            if (!held.empty() && (i % 2 == 1)) {
                Payload* p   = held.back();
                auto     idx = slot_index(p);
                held.pop_back();

                if (p->canary != Payload::kMagic || p->value != static_cast<uint64_t>(thread_id)) {
                    corruption_detected.store(true, std::memory_order_relaxed);
                }

                // Relinquish ownership before releasing back to pool.
                slot_owners[idx].store(-1, std::memory_order_release);
                pool.release(p);
            }
        }

        // Cleanup.
        for (auto* p : held) {
            auto idx = slot_index(p);
            slot_owners[idx].store(-1, std::memory_order_release);
            pool.release(p);
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(kNumThreads);

    for (int t = 0; t < static_cast<int>(kNumThreads); ++t) {
        threads.emplace_back(worker, t);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_FALSE(corruption_detected.load());
}

// ---------------------------------------------------------------------------
// 4. Pointer uniqueness under contention
//    Acquire from many threads simultaneously and verify that no pointer
//    is handed out twice.
// ---------------------------------------------------------------------------

TEST(BufferPoolTest, PointerUniqueness)
{
    constexpr std::size_t kPoolSize   = 128;
    constexpr std::size_t kNumThreads = 8;
    constexpr std::size_t kRounds     = 50'000;

    LockFreeBufferPool<Payload, kPoolSize> pool;

    std::atomic<bool> duplicate_detected{false};

    std::barrier sync_point(static_cast<std::ptrdiff_t>(kNumThreads));

    auto worker = [&]() {
        sync_point.arrive_and_wait();

        for (std::size_t round = 0; round < kRounds; ++round) {
            std::vector<Payload*> mine;

            // Grab as many as we can.
            while (Payload* p = pool.acquire()) {
                mine.push_back(p);
            }

            // Check for duplicates within our own batch.
            std::sort(mine.begin(), mine.end());
            if (std::adjacent_find(mine.begin(), mine.end()) != mine.end()) {
                duplicate_detected.store(true, std::memory_order_relaxed);
            }

            // Return everything.
            for (auto* p : mine) {
                pool.release(p);
            }
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(kNumThreads);

    for (std::size_t t = 0; t < kNumThreads; ++t) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_FALSE(duplicate_detected.load());
}

// ---------------------------------------------------------------------------
// 5. Rapid acquire-release ping-pong (maximise ABA window)
//    Two threads alternate: one acquires two, releases the first, while the
//    other does the reverse. This creates the maximal A→B→A pattern on head.
// ---------------------------------------------------------------------------

TEST(BufferPoolTest, PingPongABA)
{
    constexpr std::size_t kPoolSize   = 3;
    constexpr std::size_t kIterations = 1'000'000;

    LockFreeBufferPool<Payload, kPoolSize> pool;

    std::atomic<bool>     corruption_detected{false};
    std::atomic<uint64_t> successful_ops{0};

    // Track which slots are currently "owned" by a thread.
    std::array<std::atomic<int>, kPoolSize> slot_owners{};
    for (auto& s : slot_owners) {
        s.store(-1, std::memory_order_relaxed);
    }

    // Compute slot base address.
    Payload* base_ptr = pool.acquire();
    pool.release(base_ptr);

    auto slot_index = [base_ptr](const Payload* p) -> std::size_t {
        constexpr auto kSlotSize = std::max(sizeof(Payload), sizeof(uint32_t));
        auto           offset    = reinterpret_cast<const uint8_t*>(p) - reinterpret_cast<const uint8_t*>(base_ptr);
        return static_cast<std::size_t>(offset) / kSlotSize;
    };

    auto claim = [&](const Payload* p, int thread_id) -> bool {
        auto idx      = slot_index(p);
        int  expected = -1;
        return slot_owners[idx].compare_exchange_strong(expected, thread_id, std::memory_order_acq_rel);
    };

    auto unclaim = [&](const Payload* p) {
        auto idx = slot_index(p);
        slot_owners[idx].store(-1, std::memory_order_release);
    };

    std::barrier sync_point(2);

    auto ping = [&]() {
        sync_point.arrive_and_wait();

        for (std::size_t i = 0; i < kIterations && !corruption_detected.load(std::memory_order_relaxed); ++i) {
            Payload* a = pool.acquire();
            if (!a) {
                continue;
            }

            if (!claim(a, 0)) {
                corruption_detected.store(true, std::memory_order_relaxed);
                pool.release(a);
                break;
            }

            Payload* b = pool.acquire();
            if (b && !claim(b, 0)) {
                corruption_detected.store(true, std::memory_order_relaxed);
                unclaim(a);
                pool.release(a);
                pool.release(b);
                break;
            }

            // Release a → pool head goes back to a's slot while b is held.
            unclaim(a);
            pool.release(a);

            if (b) {
                unclaim(b);
                pool.release(b);
            }

            successful_ops.fetch_add(1, std::memory_order_relaxed);
        }
    };

    auto pong = [&]() {
        sync_point.arrive_and_wait();

        for (std::size_t i = 0; i < kIterations && !corruption_detected.load(std::memory_order_relaxed); ++i) {
            Payload* x = pool.acquire();
            if (!x) {
                continue;
            }

            if (!claim(x, 1)) {
                corruption_detected.store(true, std::memory_order_relaxed);
                pool.release(x);
                break;
            }

            // Immediately release to create rapid head churn.
            unclaim(x);
            pool.release(x);

            successful_ops.fetch_add(1, std::memory_order_relaxed);
        }
    };

    std::thread t1(ping);
    std::thread t2(pong);

    t1.join();
    t2.join();

    EXPECT_FALSE(corruption_detected.load());
    EXPECT_GT(successful_ops.load(), 0u);

    // Verify pool integrity: should get exactly kPoolSize back.
    std::size_t count = 0;

    while (pool.acquire() != nullptr) {
        ++count;
    }

    EXPECT_EQ(count, kPoolSize);
}

}  // namespace
