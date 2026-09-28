#pragma once

#include "memory/CircularQueue.h"
#include "memory/LruCache.h"
#include "memory/MemoryHashTable.h"

#include <optional>
#include <string>
#include <vector>

namespace memory {

// GUI和Agent统一调用的门面类，隐藏底层节点和内存所有权。
class MemoryManager {
public:
    explicit MemoryManager(std::size_t shortTermCapacity = 20,
                           std::size_t lruCapacity = 50);

    // 新记忆同时进入长期哈希表和短期循环队列。
    // 返回被短期队列淘汰的记忆ID；长期记忆不会因此删除。
    std::optional<std::string> add(Memory memory);
    Memory* find(const std::string& id);
    const Memory* peek(const std::string& id) const;
    bool update(const std::string& id, const std::string& content,
                int importance, std::vector<std::string> keywords = {});
    bool remove(const std::string& id);

    std::vector<Memory> all() const { return store_.values(); }
    std::vector<std::string> recentIds() const { return shortTerm_.values(); }
    std::vector<std::string> lruOrder() const { return lru_.order(); }
    std::size_t size() const noexcept { return store_.size(); }
    double hashLoadFactor() const noexcept { return store_.loadFactor(); }

    void save(const std::string& path) const;
    void load(const std::string& path);
    void clear();

private:
    MemoryHashTable store_;
    CircularQueue<std::string> shortTerm_;
    LruCache lru_;
};

}  // namespace memory

