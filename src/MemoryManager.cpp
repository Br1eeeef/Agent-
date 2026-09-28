#include "memory/MemoryManager.h"

#include "memory/JsonStorage.h"

#include <stdexcept>
#include <utility>

namespace memory {

MemoryManager::MemoryManager(std::size_t shortTermCapacity, std::size_t lruCapacity)
    : store_(31), shortTerm_(shortTermCapacity), lru_(lruCapacity) {}

std::optional<std::string> MemoryManager::add(Memory memory) {
    memory.validate();
    const std::string id = memory.id;
    if (!store_.insert(std::move(memory))) {
        throw std::invalid_argument("duplicate memory id: " + id);
    }
    try {
        lru_.touch(id);
        return shortTerm_.push(id);
    } catch (...) {
        // 保持强异常安全：索引更新失败时撤销已插入的正文。
        lru_.erase(id);
        store_.erase(id);
        throw;
    }
}

Memory* MemoryManager::find(const std::string& id) {
    Memory* value = store_.find(id);
    if (!value) return nullptr;
    value->lastAccessedAt = unixNow();
    lru_.touch(id);
    return value;
}

const Memory* MemoryManager::peek(const std::string& id) const { return store_.find(id); }

bool MemoryManager::update(const std::string& id, const std::string& content,
                           int importance, std::vector<std::string> keywords) {
    Memory* value = store_.find(id);
    if (!value) return false;
    if (content.empty()) throw std::invalid_argument("memory content must not be empty");
    if (importance < 1 || importance > 5) throw std::invalid_argument("memory importance must be between 1 and 5");
    value->content = content;
    value->importance = importance;
    value->keywords = std::move(keywords);
    value->updatedAt = unixNow();
    value->lastAccessedAt = value->updatedAt;
    lru_.touch(id);
    return true;
}

bool MemoryManager::remove(const std::string& id) {
    lru_.erase(id);
    return store_.erase(id);
}

void MemoryManager::save(const std::string& path) const { JsonStorage::save(path, store_.values()); }

void MemoryManager::load(const std::string& path) {
    auto values = JsonStorage::load(path);  // 先完整解析，失败时保留现有数据。
    MemoryManager replacement(shortTerm_.capacity(), lru_.capacity());
    for (auto& value : values) replacement.add(std::move(value));
    clear();
    for (auto& value : replacement.store_.values()) add(std::move(value));
}

void MemoryManager::clear() {
    store_.clear();
    shortTerm_.clear();
    lru_.clear();
}

}  // namespace memory

