#pragma once

#include "memory/CircularQueue.h"
#include "memory/LruCache.h"
#include "memory/MemoryHashTable.h"
#include "memory/MemoryRetriever.h"
#include "memory/Scoring.h"
#include "memory/TextAnalysis.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace memory {

// GUI和Agent统一调用的门面类，隐藏底层节点和内存所有权。
class MemoryManager {
public:
    explicit MemoryManager(std::size_t shortTermCapacity = 20,
                           std::size_t lruCapacity = 50,
                           TextAnalyzer analyzer = TextAnalyzer());

    // 新记忆同时进入长期哈希表和短期循环队列。
    // 返回被短期队列淘汰的记忆ID；长期记忆不会因此删除。
    std::optional<std::string> add(Memory memory);
    // 按ID读取并刷新LRU顺序，同时更新lastAccessedAt。
    Memory* find(const std::string& id);
    // 只读查询，不刷新LRU顺序，也不修改lastAccessedAt。
    const Memory* peek(const std::string& id) const;
    bool update(const std::string& id, const std::string& content,
                int importance, std::vector<std::string> keywords = {});
    // 只从长期哈希表与LRU索引中移除，不会清理短期循环队列中的残留ID。
    // 因此 recentIds() 可能继续返回已被删除的ID，调用方需要自行判空。
    bool remove(const std::string& id);

    // 使用最小堆检索与 input 最相关的 Top K 记忆。
    // 该方法为 const：检索本身不会触碰 LRU，只有 find()/add()/update() 会改变访问顺序。
    std::vector<MemoryScore> recall(const std::string& input, std::size_t k = 10,
                                    const ScoringWeights& weights = {}) const;

    std::vector<Memory> all() const { return store_.values(); }
    std::vector<std::string> recentIds() const { return shortTerm_.values(); }
    std::vector<std::string> lruOrder() const { return lru_.order(); }
    std::size_t size() const noexcept { return store_.size(); }
    double hashLoadFactor() const noexcept { return store_.loadFactor(); }
    // 以下只读接口供 GUI 展示真实容量与使用率，不改变任何数据结构行为。
    std::size_t shortTermCapacity() const noexcept { return shortTerm_.capacity(); }
    std::size_t shortTermSize() const noexcept { return shortTerm_.size(); }
    std::size_t lruCapacity() const noexcept { return lru_.capacity(); }
    std::size_t lruSize() const noexcept { return lru_.size(); }
    std::size_t bucketCount() const noexcept { return store_.bucketCount(); }
    const TextAnalyzer& analyzer() const noexcept { return analyzer_; }

    void save(const std::string& path) const;
    // 先完整解析目标文件，成功后整体替换现有内容；解析失败时保留原数据并抛出异常。
    // 载入过程会逐条重新 add()，因此载入后的短期队列顺序取决于存储遍历顺序。
    void load(const std::string& path);
    void clear();

private:
    MemoryHashTable store_;
    CircularQueue<std::string> shortTerm_;
    LruCache lru_;
    TextAnalyzer analyzer_;
    MemoryRetriever retriever_;
};

}  // namespace memory

