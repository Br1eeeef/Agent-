#include "memory/Scoring.h"

#include <algorithm>
#include <cmath>

namespace memory {
namespace {
constexpr double kSecondsPerDay = 86400.0;
constexpr double kRecencyHalfLifeDays = 30.0;
}  // namespace

double MemoryScorer::normalizedImportance(int level) {
    const int clamped = std::min(5, std::max(1, level));
    return static_cast<double>(clamped - 1) / 4.0;
}

double MemoryScorer::recency(std::int64_t createdAt, std::int64_t now) {
    if (createdAt <= 0) return 0.0;
    const double ageDays = static_cast<double>(now - createdAt) / kSecondsPerDay;
    const double nonNegativeAge = std::max(0.0, ageDays);
    return std::exp(-nonNegativeAge / kRecencyHalfLifeDays);
}

double MemoryScorer::combine(double relevance, double importance, double recency,
                             const ScoringWeights& weights) {
    weights.validate();
    const double sum = weights.total();
    const double value = weights.relevance * relevance + weights.importance * importance +
                         weights.recency * recency;
    return std::min(1.0, std::max(0.0, value / sum));
}

}  // namespace memory
