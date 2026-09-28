#pragma once

#include "memory/Memory.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace memory {

// 使用拉链法解决冲突的哈希表，拥有Memory对象的生命周期。
class MemoryHashTable {
public:
    explicit MemoryHashTable(std::size_t bucketCount = 31);
    ~MemoryHashTable() = default;
    MemoryHashTable(const MemoryHashTable&) = delete;
    MemoryHashTable& operator=(const MemoryHashTable&) = delete;

    bool insert(Memory memory);
    Memory* find(const std::string& id);
    const Memory* find(const std::string& id) const;
    bool erase(const std::string& id);
    void clear();
    std::vector<Memory> values() const;
    std::size_t size() const noexcept { return size_; }
    std::size_t bucketCount() const noexcept { return buckets_.size(); }
    double loadFactor() const noexcept;

private:
    struct Node {
        explicit Node(Memory value) : memory(std::move(value)) {}
        Memory memory;
        std::unique_ptr<Node> next;
    };

    std::size_t bucketIndex(const std::string& id) const;
    void rehash(std::size_t newBucketCount);

    std::vector<std::unique_ptr<Node>> buckets_;
    std::size_t size_{0};
};

}  // namespace memory
