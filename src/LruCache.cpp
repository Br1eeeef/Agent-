#include "memory/LruCache.h"

#include <functional>
#include <stdexcept>
#include <utility>

namespace memory {

LruCache::LruCache(std::size_t capacity, std::size_t bucketCount)
    : capacity_(capacity), buckets_(bucketCount == 0 ? 1 : bucketCount) {
    if (capacity == 0) throw std::invalid_argument("LRU capacity must be positive");
}

LruCache::~LruCache() { clear(); }

std::size_t LruCache::indexOf(const std::string& id) const {
    return std::hash<std::string>{}(id) % buckets_.size();
}

LruCache::ListNode* LruCache::lookup(const std::string& id) const {
    const IndexNode* entry = buckets_[indexOf(id)].get();
    while (entry != nullptr) {
        if (entry->id == id) return entry->listNode;
        entry = entry->next.get();
    }
    return nullptr;
}

void LruCache::addIndex(ListNode* node) {
    auto entry = std::make_unique<IndexNode>();
    entry->id = node->id;
    entry->listNode = node;
    auto& bucket = buckets_[indexOf(node->id)];
    entry->next = std::move(bucket);
    bucket = std::move(entry);
}

void LruCache::removeIndex(const std::string& id) {
    auto* link = &buckets_[indexOf(id)];
    while (*link) {
        if ((*link)->id == id) {
            *link = std::move((*link)->next);
            return;
        }
        link = &((*link)->next);
    }
}

void LruCache::detach(ListNode* node) {
    if (node->prev) node->prev->next = node->next; else head_ = node->next;
    if (node->next) node->next->prev = node->prev; else tail_ = node->prev;
    node->prev = nullptr;
    node->next = nullptr;
}

void LruCache::pushFront(ListNode* node) {
    node->next = head_;
    if (head_) head_->prev = node; else tail_ = node;
    head_ = node;
}

std::optional<std::string> LruCache::touch(const std::string& id) {
    if (id.empty()) throw std::invalid_argument("LRU id must not be empty");
    if (ListNode* existing = lookup(id)) {
        detach(existing);
        pushFront(existing);
        return std::nullopt;
    }

    auto* node = new ListNode(id);
    pushFront(node);
    addIndex(node);
    ++size_;

    if (size_ <= capacity_) return std::nullopt;
    ListNode* victim = tail_;
    const std::string evicted = victim->id;
    detach(victim);
    removeIndex(evicted);
    delete victim;
    --size_;
    return evicted;
}

bool LruCache::contains(const std::string& id) const { return lookup(id) != nullptr; }

bool LruCache::erase(const std::string& id) {
    ListNode* node = lookup(id);
    if (!node) return false;
    detach(node);
    removeIndex(id);
    delete node;
    --size_;
    return true;
}

void LruCache::clear() {
    ListNode* node = head_;
    while (node) {
        ListNode* next = node->next;
        delete node;
        node = next;
    }
    head_ = tail_ = nullptr;
    size_ = 0;
    for (auto& bucket : buckets_) bucket.reset();
}

std::vector<std::string> LruCache::order() const {
    std::vector<std::string> result;
    result.reserve(size_);
    for (ListNode* node = head_; node; node = node->next) result.push_back(node->id);
    return result;
}

}  // namespace memory

