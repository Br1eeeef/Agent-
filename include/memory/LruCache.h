#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace memory {

// LRU只保存记忆ID和访问顺序，记忆正文由MemoryHashTable统一拥有。
class LruCache {
public:
    explicit LruCache(std::size_t capacity, std::size_t bucketCount = 31);
    ~LruCache();
    LruCache(const LruCache&) = delete;
    LruCache& operator=(const LruCache&) = delete;

    // 访问已有ID或插入新ID；容量满时返回被淘汰的ID。
    std::optional<std::string> touch(const std::string& id);
    bool contains(const std::string& id) const;
    bool erase(const std::string& id);
    void clear();
    std::vector<std::string> order() const;  // 从最近访问到最久未访问。
    std::size_t size() const noexcept { return size_; }
    std::size_t capacity() const noexcept { return capacity_; }

private:
    struct ListNode {
        explicit ListNode(std::string value) : id(std::move(value)) {}
        std::string id;
        ListNode* prev{nullptr};
        ListNode* next{nullptr};
    };
    struct IndexNode {
        std::string id;
        ListNode* listNode{nullptr};
        std::unique_ptr<IndexNode> next;
    };

    std::size_t indexOf(const std::string& id) const;
    ListNode* lookup(const std::string& id) const;
    void addIndex(ListNode* node);
    void removeIndex(const std::string& id);
    void detach(ListNode* node);
    void pushFront(ListNode* node);

    std::size_t capacity_;
    std::size_t size_{0};
    ListNode* head_{nullptr};
    ListNode* tail_{nullptr};
    std::vector<std::unique_ptr<IndexNode>> buckets_;
};

}  // namespace memory
