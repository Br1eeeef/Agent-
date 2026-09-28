#include "memory/MemoryHashTable.h"

#include <functional>
#include <utility>

namespace memory {

MemoryHashTable::MemoryHashTable(std::size_t bucketCount)
    : buckets_(bucketCount == 0 ? 1 : bucketCount) {}

std::size_t MemoryHashTable::bucketIndex(const std::string& id) const {
    return std::hash<std::string>{}(id) % buckets_.size();
}

bool MemoryHashTable::insert(Memory memory) {
    memory.validate();
    if (find(memory.id) != nullptr) return false;
    if (loadFactor() > 0.75) rehash(buckets_.size() * 2 + 1);
    const auto index = bucketIndex(memory.id);
    auto node = std::make_unique<Node>(std::move(memory));
    node->next = std::move(buckets_[index]);
    buckets_[index] = std::move(node);
    ++size_;
    return true;
}

Memory* MemoryHashTable::find(const std::string& id) {
    Node* node = buckets_[bucketIndex(id)].get();
    while (node != nullptr) {
        if (node->memory.id == id) return &node->memory;
        node = node->next.get();
    }
    return nullptr;
}

const Memory* MemoryHashTable::find(const std::string& id) const {
    const Node* node = buckets_[bucketIndex(id)].get();
    while (node != nullptr) {
        if (node->memory.id == id) return &node->memory;
        node = node->next.get();
    }
    return nullptr;
}

bool MemoryHashTable::erase(const std::string& id) {
    auto* link = &buckets_[bucketIndex(id)];
    while (*link) {
        if ((*link)->memory.id == id) {
            *link = std::move((*link)->next);
            --size_;
            return true;
        }
        link = &((*link)->next);
    }
    return false;
}

void MemoryHashTable::clear() {
    for (auto& bucket : buckets_) bucket.reset();
    size_ = 0;
}

std::vector<Memory> MemoryHashTable::values() const {
    std::vector<Memory> result;
    result.reserve(size_);
    for (const auto& bucket : buckets_) {
        const Node* node = bucket.get();
        while (node != nullptr) {
            result.push_back(node->memory);
            node = node->next.get();
        }
    }
    return result;
}

double MemoryHashTable::loadFactor() const noexcept {
    return buckets_.empty() ? 0.0 : static_cast<double>(size_) / buckets_.size();
}

void MemoryHashTable::rehash(std::size_t newBucketCount) {
    std::vector<std::unique_ptr<Node>> old = std::move(buckets_);
    buckets_.resize(newBucketCount);
    size_ = 0;
    for (auto& bucket : old) {
        while (bucket) {
            auto next = std::move(bucket->next);
            insert(std::move(bucket->memory));
            bucket = std::move(next);
        }
    }
}

}  // namespace memory
