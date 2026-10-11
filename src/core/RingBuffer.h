#pragma once

#include <array>
#include <mutex>
#include <optional>
#include <cstddef>
#include <stdexcept>

namespace ir {

/**
 * @brief Thread-safe fixed-capacity Ring Buffer (circular queue).
 *
 * Designed for passing IR signals between worker threads (e.g., USB polling thread)
 * and consumer interfaces (QML GUI, TUI, REST API).
 */
template <typename T, size_t Capacity>
class RingBuffer {
public:
    static_assert(Capacity > 0, "Capacity must be greater than zero");

    RingBuffer() : m_head(0), m_tail(0), m_size(0) {}

    /**
     * @brief Pushes an item into the buffer.
     * If the buffer is full, the oldest item is overwritten.
     * @param item Item to insert
     * @return true if pushed without overwrite, false if an old item was overwritten.
     */
    bool push(const T& item) {
        std::lock_guard<std::mutex> lock(m_mutex);
        bool overwritten = false;
        
        m_buffer[m_head] = item;
        m_head = (m_head + 1) % Capacity;

        if (m_size == Capacity) {
            m_tail = (m_tail + 1) % Capacity;
            overwritten = true;
        } else {
            ++m_size;
        }

        return !overwritten;
    }

    /**
     * @brief Pushes an item by move semantics.
     */
    bool push(T&& item) {
        std::lock_guard<std::mutex> lock(m_mutex);
        bool overwritten = false;

        m_buffer[m_head] = std::move(item);
        m_head = (m_head + 1) % Capacity;

        if (m_size == Capacity) {
            m_tail = (m_tail + 1) % Capacity;
            overwritten = true;
        } else {
            ++m_size;
        }

        return !overwritten;
    }

    /**
     * @brief Pops the oldest item from the buffer.
     * @param outItem Reference where the popped item will be stored.
     * @return true if popped successfully, false if the buffer was empty.
     */
    bool pop(T& outItem) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_size == 0) {
            return false;
        }

        outItem = std::move(m_buffer[m_tail]);
        m_tail = (m_tail + 1) % Capacity;
        --m_size;
        return true;
    }

    /**
     * @brief Pops and returns the oldest item as std::optional.
     */
    std::optional<T> pop() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_size == 0) {
            return std::nullopt;
        }

        T item = std::move(m_buffer[m_tail]);
        m_tail = (m_tail + 1) % Capacity;
        --m_size;
        return item;
    }

    /**
     * @brief Clears all items from the buffer.
     */
    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_head = 0;
        m_tail = 0;
        m_size = 0;
    }

    /**
     * @brief Returns the number of items currently stored in the buffer.
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_size;
    }

    /**
     * @brief Returns maximum capacity of the buffer.
     */
    constexpr size_t capacity() const {
        return Capacity;
    }

    /**
     * @brief Checks if buffer is empty.
     */
    bool empty() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_size == 0;
    }

    /**
     * @brief Checks if buffer is full.
     */
    bool full() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_size == Capacity;
    }

private:
    std::array<T, Capacity> m_buffer;
    size_t m_head;
    size_t m_tail;
    size_t m_size;
    mutable std::mutex m_mutex;
};

} // namespace ir
