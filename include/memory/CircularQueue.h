#pragma once

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace memory {

// 固定容量循环队列。容量满时push会覆盖最旧元素，并返回被淘汰的值。
template <typename T>
class CircularQueue {
public:
    explicit CircularQueue(std::size_t capacity)
        : buffer_(capacity), capacity_(capacity) {
        if (capacity == 0) throw std::invalid_argument("queue capacity must be positive");
    }

    std::optional<T> push(T value) {
        std::optional<T> evicted;
        if (size_ == capacity_) {
            evicted = std::move(buffer_[head_]);
            buffer_[head_] = std::move(value);
            head_ = (head_ + 1) % capacity_;
        } else {
            buffer_[(head_ + size_) % capacity_] = std::move(value);
            ++size_;
        }
        return evicted;
    }

    T pop() {
        if (empty()) throw std::out_of_range("cannot pop from an empty queue");
        T value = std::move(buffer_[head_]);
        head_ = (head_ + 1) % capacity_;
        --size_;
        return value;
    }

    const T& front() const {
        if (empty()) throw std::out_of_range("cannot read an empty queue");
        return buffer_[head_];
    }

    std::vector<T> values() const {
        std::vector<T> result;
        result.reserve(size_);
        for (std::size_t i = 0; i < size_; ++i) result.push_back(buffer_[(head_ + i) % capacity_]);
        return result;
    }

    void clear() { head_ = 0; size_ = 0; }
    bool empty() const noexcept { return size_ == 0; }
    std::size_t size() const noexcept { return size_; }
    std::size_t capacity() const noexcept { return capacity_; }

private:
    std::vector<T> buffer_;
    std::size_t capacity_;
    std::size_t head_{0};
    std::size_t size_{0};
};

}  // namespace memory

