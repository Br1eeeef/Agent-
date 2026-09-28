#include "memory/JsonStorage.h"

#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace memory {
namespace {

std::string escapeJson(const std::string& value) {
    std::string result;
    for (unsigned char ch : value) {
        switch (ch) {
            case '\\': result += "\\\\"; break;
            case '"': result += "\\\""; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (ch < 0x20) throw std::invalid_argument("control character in JSON string");
                result += static_cast<char>(ch);
        }
    }
    return result;
}

class Parser {
public:
    explicit Parser(std::string text) : text_(std::move(text)) {}

    std::vector<Memory> parseDocument() {
        skip(); expect('{');
        const std::string key = parseString();
        if (key != "memories") fail("expected memories field");
        skip(); expect(':'); skip(); expect('[');
        std::vector<Memory> result;
        skip();
        if (peek() != ']') {
            while (true) {
                result.push_back(parseMemory());
                skip();
                if (peek() == ']') break;
                expect(',');
            }
        }
        expect(']'); skip(); expect('}'); skip();
        if (pos_ != text_.size()) fail("unexpected trailing data");
        return result;
    }

private:
    Memory parseMemory() {
        expect('{');
        std::unordered_map<std::string, std::string> strings;
        std::unordered_map<std::string, std::int64_t> numbers;
        std::vector<std::string> keywords;
        while (true) {
            skip();
            const std::string key = parseString();
            skip(); expect(':'); skip();
            if (key == "keywords") keywords = parseStringArray();
            else if (key == "importance" || key == "createdAt" || key == "updatedAt" || key == "lastAccessedAt") numbers[key] = parseInteger();
            else strings[key] = parseString();
            skip();
            if (peek() == '}') break;
            expect(',');
        }
        expect('}');
        const char* required[] = {"id", "content", "type"};
        for (const char* key : required) if (!strings.count(key)) fail(std::string("missing field: ") + key);
        const char* requiredNumbers[] = {"importance", "createdAt", "updatedAt", "lastAccessedAt"};
        for (const char* key : requiredNumbers) if (!numbers.count(key)) fail(std::string("missing field: ") + key);
        Memory value{strings["id"], strings["content"], memoryTypeFromString(strings["type"]),
                     static_cast<int>(numbers["importance"]), numbers["createdAt"], numbers["updatedAt"],
                     numbers["lastAccessedAt"], std::move(keywords)};
        value.validate();
        return value;
    }

    std::vector<std::string> parseStringArray() {
        expect('['); skip();
        std::vector<std::string> result;
        if (peek() != ']') {
            while (true) {
                result.push_back(parseString()); skip();
                if (peek() == ']') break;
                expect(',');
            }
        }
        expect(']');
        return result;
    }

    std::string parseString() {
        skip(); expect('"');
        std::string result;
        while (pos_ < text_.size()) {
            char ch = text_[pos_++];
            if (ch == '"') return result;
            if (ch != '\\') { result += ch; continue; }
            if (pos_ >= text_.size()) fail("unterminated escape");
            switch (text_[pos_++]) {
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                default: fail("unsupported escape sequence");
            }
        }
        fail("unterminated string");
        return {};
    }

    std::int64_t parseInteger() {
        skip();
        std::size_t begin = pos_;
        if (peek() == '-') ++pos_;
        while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) ++pos_;
        if (begin == pos_ || (text_[begin] == '-' && begin + 1 == pos_)) fail("expected integer");
        return std::stoll(text_.substr(begin, pos_ - begin));
    }

    void skip() { while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) ++pos_; }
    char peek() const { return pos_ < text_.size() ? text_[pos_] : '\0'; }
    void expect(char wanted) { skip(); if (peek() != wanted) fail(std::string("expected '") + wanted + "'"); ++pos_; }
    [[noreturn]] void fail(const std::string& message) const { throw std::runtime_error("invalid JSON at offset " + std::to_string(pos_) + ": " + message); }

    std::string text_;
    std::size_t pos_{0};
};

}  // namespace

void JsonStorage::save(const std::string& path, const std::vector<Memory>& memories) {
    const std::string temporary = path + ".tmp";
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("cannot open file for writing: " + temporary);
    output << "{\n  \"memories\": [\n";
    for (std::size_t i = 0; i < memories.size(); ++i) {
        const auto& m = memories[i];
        m.validate();
        output << "    {\"id\":\"" << escapeJson(m.id) << "\",\"content\":\"" << escapeJson(m.content)
               << "\",\"type\":\"" << toString(m.type) << "\",\"importance\":" << m.importance
               << ",\"createdAt\":" << m.createdAt << ",\"updatedAt\":" << m.updatedAt
               << ",\"lastAccessedAt\":" << m.lastAccessedAt << ",\"keywords\":[";
        for (std::size_t k = 0; k < m.keywords.size(); ++k) {
            if (k) output << ',';
            output << '"' << escapeJson(m.keywords[k]) << '"';
        }
        output << "]}" << (i + 1 == memories.size() ? "\n" : ",\n");
    }
    output << "  ]\n}\n";
    output.close();
    if (!output) throw std::runtime_error("failed while writing file: " + temporary);
    std::remove(path.c_str());
    if (std::rename(temporary.c_str(), path.c_str()) != 0) throw std::runtime_error("cannot replace data file: " + path);
}

std::vector<Memory> JsonStorage::load(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open file for reading: " + path);
    std::ostringstream text;
    text << input.rdbuf();
    if (!input.good() && !input.eof()) throw std::runtime_error("failed while reading file: " + path);
    return Parser(text.str()).parseDocument();
}

}  // namespace memory
