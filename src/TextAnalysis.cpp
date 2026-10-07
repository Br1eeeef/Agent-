#include "memory/TextAnalysis.h"

#include <cctype>
#include <cstdint>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace memory {
namespace {

bool isAsciiWordChar(unsigned char ch) {
    return std::isalnum(ch) != 0 || ch == '_';
}

// 判断一个 Unicode 码点是否属于需要按字/二元组处理的 CJK 区段。
bool isCjk(std::uint32_t code) {
    return (code >= 0x4E00 && code <= 0x9FFF) ||  // CJK 基本区
           (code >= 0x3400 && code <= 0x4DBF) ||  // 扩展 A
           (code >= 0xF900 && code <= 0xFAFF) ||  // 兼容表意文字
           (code >= 0x3040 && code <= 0x30FF) ||  // 日文假名
           (code >= 0xAC00 && code <= 0xD7AF);    // 韩文音节
}

std::string encodeUtf8(std::uint32_t code) {
    std::string result;
    if (code < 0x80) {
        result += static_cast<char>(code);
    } else if (code < 0x800) {
        result += static_cast<char>(0xC0 | (code >> 6));
        result += static_cast<char>(0x80 | (code & 0x3F));
    } else if (code < 0x10000) {
        result += static_cast<char>(0xE0 | (code >> 12));
        result += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
        result += static_cast<char>(0x80 | (code & 0x3F));
    } else {
        result += static_cast<char>(0xF0 | (code >> 18));
        result += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
        result += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
        result += static_cast<char>(0x80 | (code & 0x3F));
    }
    return result;
}

// 解码一个 UTF-8 码点，返回消耗的字节数。非法字节按单字节处理。
std::size_t decodeUtf8(const std::string& text, std::size_t offset, std::uint32_t& code) {
    const unsigned char first = static_cast<unsigned char>(text[offset]);
    if (first < 0x80) {
        code = first;
        return 1;
    }
    std::size_t length = 0;
    std::uint32_t value = 0;
    if ((first & 0xE0) == 0xC0) {
        length = 2;
        value = first & 0x1F;
    } else if ((first & 0xF0) == 0xE0) {
        length = 3;
        value = first & 0x0F;
    } else if ((first & 0xF8) == 0xF0) {
        length = 4;
        value = first & 0x07;
    } else {
        code = first;
        return 1;
    }
    if (offset + length > text.size()) {
        code = first;
        return 1;
    }
    for (std::size_t i = 1; i < length; ++i) {
        const unsigned char next = static_cast<unsigned char>(text[offset + i]);
        if ((next & 0xC0) != 0x80) {
            code = first;
            return 1;
        }
        value = (value << 6) | (next & 0x3F);
    }
    code = value;
    return length;
}

}  // namespace

std::vector<std::string> SimpleTokenizer::tokenize(const std::string& text) const {
    std::vector<std::string> tokens;
    std::string ascii;                 // 当前累积的 ASCII 词
    std::vector<std::string> cjkRun;   // 当前累积的 CJK 字符

    const auto flushAscii = [&]() {
        if (!ascii.empty()) {
            tokens.push_back(ascii);
            ascii.clear();
        }
    };
    const auto flushCjk = [&]() {
        for (std::size_t i = 0; i < cjkRun.size(); ++i) {
            tokens.push_back(cjkRun[i]);  // 单字
            if (i + 1 < cjkRun.size()) tokens.push_back(cjkRun[i] + cjkRun[i + 1]);  // 二元组
        }
        cjkRun.clear();
    };

    for (std::size_t i = 0; i < text.size();) {
        const unsigned char ch = static_cast<unsigned char>(text[i]);
        if (ch < 0x80) {
            flushCjk();
            if (isAsciiWordChar(ch)) {
                ascii += static_cast<char>(std::tolower(ch));
            } else {
                flushAscii();
            }
            ++i;
            continue;
        }
        flushAscii();
        std::uint32_t code = 0;
        const std::size_t length = decodeUtf8(text, i, code);
        if (isCjk(code)) {
            cjkRun.push_back(encodeUtf8(code));
        } else {
            flushCjk();
        }
        i += length;
    }
    flushAscii();
    flushCjk();
    return tokens;
}

double JaccardRelevance::relevance(const std::vector<std::string>& queryTokens,
                                   const std::vector<std::string>& memoryTokens) const {
    if (queryTokens.empty() || memoryTokens.empty()) return 0.0;
    std::unordered_set<std::string> query(queryTokens.begin(), queryTokens.end());
    std::unordered_set<std::string> target(memoryTokens.begin(), memoryTokens.end());
    std::size_t intersection = 0;
    for (const auto& token : query) {
        if (target.count(token)) ++intersection;
    }
    const std::size_t unionSize = query.size() + target.size() - intersection;
    if (unionSize == 0) return 0.0;
    return static_cast<double>(intersection) / static_cast<double>(unionSize);
}

TextAnalyzer::TextAnalyzer()
    : tokenizer_(std::make_shared<SimpleTokenizer>()),
      relevance_(std::make_shared<JaccardRelevance>()) {}

TextAnalyzer::TextAnalyzer(std::shared_ptr<const Tokenizer> tokenizer,
                           std::shared_ptr<const RelevanceStrategy> relevance)
    : tokenizer_(std::move(tokenizer)), relevance_(std::move(relevance)) {
    if (!tokenizer_ || !relevance_) {
        throw std::invalid_argument("TextAnalyzer requires a tokenizer and a relevance strategy");
    }
}

std::vector<std::string> TextAnalyzer::tokenize(const std::string& text) const {
    return tokenizer_->tokenize(text);
}

double TextAnalyzer::relevance(const std::string& query,
                               const std::vector<std::string>& memoryTokens) const {
    return relevance_->relevance(tokenizer_->tokenize(query), memoryTokens);
}

}  // namespace memory
