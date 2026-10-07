#include "memory/MemoryRetriever.h"

#include "memory/MinHeap.h"

#include <algorithm>
#include <utility>

namespace memory {
namespace {

// 最小堆的“更差者更小”：堆顶是应被淘汰的候选（得分最低，同分时 id 较大）。
struct WorseFirst {
    bool operator()(const MemoryScore& left, const MemoryScore& right) const {
        if (left.score != right.score) return left.score < right.score;
        return left.memory.id > right.memory.id;
    }
};

}  // namespace

MemoryRetriever::MemoryRetriever(TextAnalyzer analyzer) : analyzer_(std::move(analyzer)) {}

std::vector<MemoryScore> MemoryRetriever::retrieve(const std::vector<Memory>& memories,
                                                   const std::string& input, std::size_t k,
                                                   const ScoringWeights& weights,
                                                   std::int64_t now) const {
    if (k == 0 || memories.empty()) return {};
    weights.validate();
    const std::int64_t reference = now > 0 ? now : unixNow();
    const auto queryTokens = analyzer_.tokenize(input);

    MinHeap<MemoryScore, WorseFirst> heap;
    for (const auto& item : memories) {
        MemoryScore entry;
        entry.memory = item;
        // 录入时已分词；若调用方直接构造 Memory 未分词，则在这里补一次。
        if (entry.memory.keywords.empty()) {
            entry.memory.keywords = analyzer_.tokenize(item.content);
        }
        const std::vector<std::string>& tokens = entry.memory.keywords;
        entry.relevance = analyzer_.relevanceStrategy().relevance(queryTokens, tokens);
        entry.importance = MemoryScorer::normalizedImportance(item.importance);
        entry.recency = MemoryScorer::recency(item.createdAt, reference);
        entry.score =
            MemoryScorer::combine(entry.relevance, entry.importance, entry.recency, weights);

        heap.push(std::move(entry));
        if (heap.size() > k) heap.pop();
    }

    std::vector<MemoryScore> result = heap.values();
    std::sort(result.begin(), result.end(), [](const MemoryScore& left, const MemoryScore& right) {
        if (left.score != right.score) return left.score > right.score;
        return left.memory.id < right.memory.id;
    });
    return result;
}

}  // namespace memory
