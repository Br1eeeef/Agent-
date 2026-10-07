#pragma once

#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace memory {

// 通用二叉最小堆。堆顶始终是 Compare 意义下最小的元素。
// Top-K 检索时把 Compare 定义成“更差者更小”，堆顶即为应被淘汰的候选。
template <typename T, typename Compare = std::less<T>>
class MinHeap {
public:
    MinHeap() = default;
    explicit MinHeap(Compare compare) : compare_(std::move(compare)) {}

    void push(T value) {
        data_.push_back(std::move(value));
        siftUp(data_.size() - 1);
    }

    const T& top() const { return data_.front(); }

    T pop() {
        T result = std::move(data_.front());
        data_.front() = std::move(data_.back());
        data_.pop_back();
        if (!data_.empty()) siftDown(0);
        return result;
    }

    void clear() { data_.clear(); }
    bool empty() const noexcept { return data_.empty(); }
    std::size_t size() const noexcept { return data_.size(); }
    const std::vector<T>& values() const noexcept { return data_; }

private:
    void siftUp(std::size_t index) {
        while (index > 0) {
            const std::size_t parent = (index - 1) / 2;
            if (!compare_(data_[index], data_[parent])) break;
            std::swap(data_[index], data_[parent]);
            index = parent;
        }
    }

    void siftDown(std::size_t index) {
        const std::size_t count = data_.size();
        while (true) {
            const std::size_t left = index * 2 + 1;
            const std::size_t right = left + 1;
            std::size_t best = index;
            if (left < count && compare_(data_[left], data_[best])) best = left;
            if (right < count && compare_(data_[right], data_[best])) best = right;
            if (best == index) break;
            std::swap(data_[index], data_[best]);
            index = best;
        }
    }

    std::vector<T> data_;
    Compare compare_{};
};

}  // namespace memory
