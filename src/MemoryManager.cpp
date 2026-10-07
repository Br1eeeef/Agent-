#include "memory/MemoryManager.h"

#include "memory/JsonStorage.h"

#include <stdexcept>
#include <utility>

namespace memory {

MemoryManager::MemoryManager(std::size_t shortTermCapacity, std::size_t lruCapacity,
                             TextAnalyzer analyzer)
    : store_(31),
      shortTerm_(shortTermCapacity),
      lru_(lruCapacity),
      analyzer_(std::move(analyzer)),
      retriever_(analyzer_) {}

std::optional<std::string> MemoryManager::add(Memory memory) {
    memory.validate();
    // 录入时即完成分词，避免每次查询重复计算。
    if (memory.keywords.empty()) memory.keywords = analyzer_.tokenize(memory.content);
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
    // 未显式给出关键词时按新正文重新分词。
    value->keywords = keywords.empty() ? analyzer_.tokenize(content) : std::move(keywords);
    value->updatedAt = unixNow();
    value->lastAccessedAt = value->updatedAt;
    lru_.touch(id);
    return true;
}

std::vector<MemoryScore> MemoryManager::recall(const std::string& input, std::size_t k,
                                               const ScoringWeights& weights) const {
    return retriever_.retrieve(store_.values(), input, k, weights);
}

bool MemoryManager::remove(const std::string& id) {
    lru_.erase(id);
    return store_.erase(id);
}

void MemoryManager::save(const std::string& path) const { JsonStorage::save(path, store_.values()); }

void MemoryManager::load(const std::string& path) {
    auto values = JsonStorage::load(path);  // 先完整解析，失败时保留现有数据。
    MemoryManager replacement(shortTerm_.capacity(), lru_.capacity(), analyzer_);
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

