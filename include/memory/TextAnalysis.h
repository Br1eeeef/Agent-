#pragma once

#include <memory>
#include <string>
#include <vector>

namespace memory {

// 分词策略接口。评分子系统只依赖该抽象，便于把默认实现替换成
// 词典分词、BPE 或模型分词而不影响其他模块。
class Tokenizer {
public:
    virtual ~Tokenizer() = default;
    virtual std::vector<std::string> tokenize(const std::string& text) const = 0;
};

// 默认分词器：ASCII 字母数字按词切分并统一小写；CJK 连续段同时输出
// 单字与相邻二元组，这样无需词典也能对中文做粗略的相似度比较。
class SimpleTokenizer : public Tokenizer {
public:
    std::vector<std::string> tokenize(const std::string& text) const override;
};

// 相关度策略接口。默认使用集合 Jaccard，可替换为向量余弦、BM25 等。
class RelevanceStrategy {
public:
    virtual ~RelevanceStrategy() = default;
    virtual double relevance(const std::vector<std::string>& queryTokens,
                             const std::vector<std::string>& memoryTokens) const = 0;
};

// 分词后集合的 Jaccard 相似度：|交集| / |并集|，取值 0 到 1。
class JaccardRelevance : public RelevanceStrategy {
public:
    double relevance(const std::vector<std::string>& queryTokens,
                     const std::vector<std::string>& memoryTokens) const override;
};

// 把分词器与相关度策略组合在一起，供检索模块以低耦合方式使用。
class TextAnalyzer {
public:
    TextAnalyzer();
    TextAnalyzer(std::shared_ptr<const Tokenizer> tokenizer,
                 std::shared_ptr<const RelevanceStrategy> relevance);

    std::vector<std::string> tokenize(const std::string& text) const;
    double relevance(const std::string& query,
                     const std::vector<std::string>& memoryTokens) const;

    const Tokenizer& tokenizer() const { return *tokenizer_; }
    const RelevanceStrategy& relevanceStrategy() const { return *relevance_; }

private:
    std::shared_ptr<const Tokenizer> tokenizer_;
    std::shared_ptr<const RelevanceStrategy> relevance_;
};

}  // namespace memory
