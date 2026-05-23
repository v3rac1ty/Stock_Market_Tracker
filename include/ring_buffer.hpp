#pragma once
#include <atomic>
#include <array>
#include <optional>
#include <cstddef>

// Lock-free single-producer / single-consumer ring buffer.
// Capacity must be a power of 2.  Head and tail on separate cache lines
// to avoid false sharing between producer and consumer threads.
template<typename T, size_t Capacity>
class RingBuffer {
    static_assert(Capacity >= 2 && (Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of 2");
public:
    // Producer side — returns false when full.
    bool push(T item) noexcept {
        const size_t head = m_head.load(std::memory_order_relaxed);
        const size_t next = (head + 1) & kMask;
        if (next == m_tail.load(std::memory_order_acquire))
            return false;
        m_data[head] = std::move(item);
        m_head.store(next, std::memory_order_release);
        return true;
    }

    // Consumer side — returns nullopt when empty.
    std::optional<T> pop() noexcept {
        const size_t tail = m_tail.load(std::memory_order_relaxed);
        if (tail == m_head.load(std::memory_order_acquire))
            return std::nullopt;
        T item = std::move(m_data[tail]);
        m_tail.store((tail + 1) & kMask, std::memory_order_release);
        return item;
    }

    bool empty() const noexcept {
        return m_tail.load(std::memory_order_relaxed) ==
               m_head.load(std::memory_order_relaxed);
    }

    static constexpr size_t capacity() noexcept { return Capacity - 1; }

private:
    static constexpr size_t kMask = Capacity - 1;

    alignas(64) std::atomic<size_t> m_head{0};
    alignas(64) std::atomic<size_t> m_tail{0};
    std::array<T, Capacity> m_data{};
};
