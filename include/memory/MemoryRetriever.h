#pragma once

#include "memory/Scoring.h"
#include "memory/TextAnalysis.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace memory {

// 基于最小堆的 Top-K 召回。时间复杂度 O(n log k)，只保留得分最高的 k 条。
class MemoryRetriever {
public:
    explicit MemoryRetriever(TextAnalyzer analyzer = TextAnalyzer());

    // 计算每条记忆的 score 并返回前 k 大，按得分从高到低排序。
    // now 传 0 时使用当前系统时间。
    std::vector<MemoryScore> retrieve(const std::vector<Memory>& memories,
                                      const std::string& input,
                                      std::size_t k = 10,
                                      const ScoringWeights& weights = {},
                                      std::int64_t now = 0) const;

    const TextAnalyzer& analyzer() const { return analyzer_; }

private:
    TextAnalyzer analyzer_;
};

}  // namespace memory
