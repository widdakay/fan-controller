#pragma once
#include <array>
#include <cstddef>

namespace util {

// Fixed-size ring buffer for embedded systems
template<typename T, size_t Capacity>
class RingBuffer {
public:
    RingBuffer() : head_(0), tail_(0), size_(0) {}

    // Add element to buffer (overwrites oldest if full)
    void push(const T& item) {
        buffer_[head_] = item;
        head_ = (head_ + 1) % Capacity;

        if (size_ < Capacity) {
            size_++;
        } else {
            // Buffer full, overwrite oldest
            tail_ = (tail_ + 1) % Capacity;
        }
    }

    void push(T&& item) {
        buffer_[head_] = std::move(item);
        head_ = (head_ + 1) % Capacity;

        if (size_ < Capacity) {
            size_++;
        } else {
            tail_ = (tail_ + 1) % Capacity;
        }
    }

    // Remove and return oldest element
    bool pop(T& out) {
        if (empty()) {
            return false;
        }

        out = std::move(buffer_[tail_]);
        tail_ = (tail_ + 1) % Capacity;
        size_--;
        return true;
    }

    // Peek at oldest element without removing
    const T* peek() const {
        if (empty()) {
            return nullptr;
        }
        return &buffer_[tail_];
    }

    // Query methods
    bool empty() const { return size_ == 0; }
    bool full() const { return size_ == Capacity; }
    size_t size() const { return size_; }
    size_t capacity() const { return Capacity; }

    // Clear all elements
    void clear() {
        head_ = 0;
        tail_ = 0;
        size_ = 0;
    }

    // Access element at index (0 = oldest)
    const T& operator[](size_t idx) const {
        return buffer_[(tail_ + idx) % Capacity];
    }

private:
    std::array<T, Capacity> buffer_;
    size_t head_;  // Next write position
    size_t tail_;  // Next read position
    size_t size_;  // Current number of elements
};

} // namespace util
