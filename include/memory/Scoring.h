#pragma once

#include "memory/Memory.h"

#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace memory {

// 召回公式权重：score = wR*relevance + wI*importance + wN*recency。
struct ScoringWeights {
    double relevance{0.5};
    double importance{0.3};
    double recency{0.2};

    double total() const noexcept { return relevance + importance + recency; }

    void validate() const {
        if (relevance < 0.0 || importance < 0.0 || recency < 0.0) {
            throw std::invalid_argument("scoring weights must not be negative");
        }
        if (total() <= 0.0) {
            throw std::invalid_argument("scoring weights must sum to a positive value");
        }
    }
};

// 单条记忆的评分结果。保存记忆副本，避免把容器内部指针暴露给调用方。
struct MemoryScore {
    Memory memory;
    double score{0.0};
    double relevance{0.0};
    double importance{0.0};
    double recency{0.0};
};

// 评分项计算。三项都归一化到 0 到 1。
class MemoryScorer {
public:
    // importance = (level - 1) / 4，level 限制在 1 到 5。
    static double normalizedImportance(int level);
    // recency = exp(-ageDays / 30)，ageDays 由 createdAt 与 now 计算。
    static double recency(std::int64_t createdAt, std::int64_t now);
    // 按权重线性组合，并按权重和归一化，保证结果落在 0 到 1。
    static double combine(double relevance, double importance, double recency,
                          const ScoringWeights& weights);
};

}  // namespace memory
