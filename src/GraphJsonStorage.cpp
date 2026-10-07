#include "memory/GraphJsonStorage.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace memory {
namespace {

// 极简 JSON 值，仅服务于本文件的解析与序列化。
struct Json {
    enum class Kind { Null, Bool, Number, String, Array, Object };
    Kind kind{Kind::Null};
    bool boolean{false};
    double number{0.0};
    std::string text;
    std::vector<Json> array;
    std::vector<std::pair<std::string, Json>> object;

    const Json* find(const std::string& key) const {
        if (kind != Kind::Object) return nullptr;
        for (const auto& entry : object) {
            if (entry.first == key) return &entry.second;
        }
        return nullptr;
    }

    const Json& at(const std::string& key) const {
        const Json* value = find(key);
        if (value == nullptr) throw std::runtime_error("missing JSON field: " + key);
        return *value;
    }

    std::string asString() const {
        if (kind != Kind::String) throw std::runtime_error("expected JSON string");
        return text;
    }
    std::int64_t asInt() const {
        if (kind != Kind::Number) throw std::runtime_error("expected JSON number");
        return static_cast<std::int64_t>(number);
    }
    double asDouble() const {
        if (kind != Kind::Number) throw std::runtime_error("expected JSON number");
        return number;
    }
    bool asBool() const {
        if (kind != Kind::Bool) throw std::runtime_error("expected JSON bool");
        return boolean;
    }
    std::vector<std::string> asStringArray() const {
        if (kind != Kind::Array) throw std::runtime_error("expected JSON array");
        std::vector<std::string> result;
        result.reserve(array.size());
        for (const auto& value : array) result.push_back(value.asString());
        return result;
    }

    std::int64_t intOr(const std::string& key, std::int64_t fallback) const {
        const Json* value = find(key);
        return value != nullptr && value->kind == Kind::Number ? value->asInt() : fallback;
    }
    double doubleOr(const std::string& key, double fallback) const {
        const Json* value = find(key);
        return value != nullptr && value->kind == Kind::Number ? value->number : fallback;
    }
    bool boolOr(const std::string& key, bool fallback) const {
        const Json* value = find(key);
        return value != nullptr && value->kind == Kind::Bool ? value->boolean : fallback;
    }
    std::string stringOr(const std::string& key, const std::string& fallback) const {
        const Json* value = find(key);
        return value != nullptr && value->kind == Kind::String ? value->text : fallback;
    }
    std::vector<std::string> stringArrayOr(const std::string& key) const {
        const Json* value = find(key);
        if (value == nullptr || value->kind != Kind::Array) return {};
        return value->asStringArray();
    }
};

class JsonParser {
public:
    explicit JsonParser(std::string text) : text_(std::move(text)) {}

    Json parse() {
        skip();
        Json value = parseValue();
        skip();
        if (pos_ != text_.size()) fail("unexpected trailing data");
        return value;
    }

private:
    Json parseValue() {
        skip();
        if (pos_ >= text_.size()) fail("unexpected end of input");
        switch (text_[pos_]) {
            case '{': return parseObject();
            case '[': return parseArray();
            case '"': {
                Json value;
                value.kind = Json::Kind::String;
                value.text = parseString();
                return value;
            }
            case 't': {
                expectWord("true");
                Json value;
                value.kind = Json::Kind::Bool;
                value.boolean = true;
                return value;
            }
            case 'f': {
                expectWord("false");
                Json value;
                value.kind = Json::Kind::Bool;
                value.boolean = false;
                return value;
            }
            case 'n':
                expectWord("null");
                return Json{};
            default:
                return parseNumber();
        }
    }

    Json parseObject() {
        Json value;
        value.kind = Json::Kind::Object;
        expect('{');
        skip();
        if (peek() == '}') {
            ++pos_;
            return value;
        }
        while (true) {
            skip();
            const std::string key = parseString();
            skip();
            expect(':');
            value.object.emplace_back(key, parseValue());
            skip();
            if (peek() == ',') {
                ++pos_;
                continue;
            }
            expect('}');
            break;
        }
        return value;
    }

    Json parseArray() {
        Json value;
        value.kind = Json::Kind::Array;
        expect('[');
        skip();
        if (peek() == ']') {
            ++pos_;
            return value;
        }
        while (true) {
            value.array.push_back(parseValue());
            skip();
            if (peek() == ',') {
                ++pos_;
                continue;
            }
            expect(']');
            break;
        }
        return value;
    }

    Json parseNumber() {
        skip();
        const std::size_t begin = pos_;
        if (peek() == '-') ++pos_;
        while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) ++pos_;
        if (peek() == '.') {
            ++pos_;
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
                ++pos_;
            }
        }
        if (peek() == 'e' || peek() == 'E') {
            ++pos_;
            if (peek() == '+' || peek() == '-') ++pos_;
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
                ++pos_;
            }
        }
        if (begin == pos_) fail("expected number");
        Json value;
        value.kind = Json::Kind::Number;
        value.number = std::stod(text_.substr(begin, pos_ - begin));
        return value;
    }

    std::string parseString() {
        skip();
        expect('"');
        std::string result;
        while (pos_ < text_.size()) {
            const char ch = text_[pos_++];
            if (ch == '"') return result;
            if (ch != '\\') {
                result += ch;
                continue;
            }
            if (pos_ >= text_.size()) fail("unterminated escape");
            switch (text_[pos_++]) {
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case '/': result += '/'; break;
                case 'b': result += '\b'; break;
                case 'f': result += '\f'; break;
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                case 'u': result += parseUnicodeEscape(); break;
                default: fail("unsupported escape sequence");
            }
        }
        fail("unterminated string");
        return {};
    }

    std::string parseUnicodeEscape() {
        std::uint32_t code = readHex4();
        if (code >= 0xD800 && code <= 0xDBFF && pos_ + 1 < text_.size() && text_[pos_] == '\\' &&
            text_[pos_ + 1] == 'u') {
            pos_ += 2;
            const std::uint32_t low = readHex4();
            if (low >= 0xDC00 && low <= 0xDFFF) {
                code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
            }
        }
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

    std::uint32_t readHex4() {
        std::uint32_t value = 0;
        for (int i = 0; i < 4; ++i) {
            if (pos_ >= text_.size()) fail("truncated unicode escape");
            const char ch = text_[pos_++];
            value <<= 4;
            if (ch >= '0' && ch <= '9') value |= static_cast<std::uint32_t>(ch - '0');
            else if (ch >= 'a' && ch <= 'f') value |= static_cast<std::uint32_t>(ch - 'a' + 10);
            else if (ch >= 'A' && ch <= 'F') value |= static_cast<std::uint32_t>(ch - 'A' + 10);
            else fail("invalid hex digit in unicode escape");
        }
        return value;
    }

    void expectWord(const char* word) {
        const std::size_t length = std::char_traits<char>::length(word);
        if (text_.compare(pos_, length, word) != 0) fail("unexpected literal");
        pos_ += length;
    }

    void skip() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) ++pos_;
    }
    char peek() const { return pos_ < text_.size() ? text_[pos_] : '\0'; }
    void expect(char wanted) {
        skip();
        if (peek() != wanted) fail(std::string("expected '") + wanted + "'");
        ++pos_;
    }
    [[noreturn]] void fail(const std::string& message) const {
        throw std::runtime_error("invalid JSON at offset " + std::to_string(pos_) + ": " + message);
    }

    std::string text_;
    std::size_t pos_{0};
};

void appendEscaped(std::string& out, const std::string& value) {
    for (unsigned char ch : value) {
        switch (ch) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (ch < 0x20) throw std::invalid_argument("control character in JSON string");
                out += static_cast<char>(ch);
        }
    }
}

std::string quoted(const std::string& value) {
    std::string out = "\"";
    appendEscaped(out, value);
    out += '"';
    return out;
}

std::string stringArray(const std::vector<std::string>& values) {
    std::string out = "[";
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) out += ',';
        out += quoted(values[i]);
    }
    out += ']';
    return out;
}

Entity parseEntity(const Json& value) {
    Entity entity;
    entity.id = value.at("id").asString();
    entity.name = value.at("name").asString();
    entity.type = entityTypeFromString(value.at("type").asString());
    entity.aliases = value.stringArrayOr("aliases");
    entity.description = value.stringOr("description", "");
    entity.sourceMemoryId = value.stringOr("sourceMemoryId", "");
    entity.memoryIds = value.stringArrayOr("memoryIds");
    entity.createdAt = value.intOr("createdAt", 0);
    entity.updatedAt = value.intOr("updatedAt", entity.createdAt);
    entity.active = value.boolOr("active", true);
    entity.invalidAt = value.intOr("invalidAt", 0);
    entity.invalidReason = value.stringOr("invalidReason", "");
    entity.priority = static_cast<int>(value.intOr("priority", 3));
    if (!entity.sourceMemoryId.empty() && entity.memoryIds.empty()) {
        entity.memoryIds.push_back(entity.sourceMemoryId);
    }
    entity.validate();
    return entity;
}

Edge parseEdge(const Json& value) {
    Edge edge;
    edge.id = value.at("id").asString();
    edge.fromEntityId = value.at("fromEntityId").asString();
    edge.toEntityId = value.at("toEntityId").asString();
    edge.relation = value.at("relation").asString();
    edge.description = value.stringOr("description", "");
    edge.eventTime = value.intOr("eventTime", 0);
    edge.createdAt = value.intOr("createdAt", edge.eventTime);
    edge.validFrom = value.intOr("validFrom", edge.eventTime);
    edge.validTo = value.intOr("validTo", 0);
    edge.memoryIds = value.stringArrayOr("memoryIds");
    edge.source = edgeSourceFromString(value.stringOr("source", "extractor"));
    edge.active = value.boolOr("active", true);
    edge.invalidAt = value.intOr("invalidAt", 0);
    edge.invalidReason = value.stringOr("invalidReason", "");
    edge.confidence = value.doubleOr("confidence", 1.0);
    edge.priority = static_cast<int>(value.intOr("priority", 3));
    edge.validate();
    return edge;
}

}  // namespace

void GraphJsonStorage::save(const std::string& path, const GraphStore& store) {
    const std::string temporary = path + ".tmp";
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("cannot open file for writing: " + temporary);

    const GraphStore::Snapshot snapshot = store.capture();
    std::vector<Entity> entities;
    entities.reserve(snapshot.entities.size());
    for (const auto& entry : snapshot.entities) entities.push_back(entry.second);
    std::sort(entities.begin(), entities.end(),
              [](const Entity& left, const Entity& right) { return left.id < right.id; });
    std::vector<Edge> edges;
    edges.reserve(snapshot.edges.size());
    for (const auto& entry : snapshot.edges) edges.push_back(entry.second);
    std::sort(edges.begin(), edges.end(),
              [](const Edge& left, const Edge& right) { return left.id < right.id; });

    output << "{\n  \"entitySeq\": " << snapshot.entitySeq << ",\n"
           << "  \"edgeSeq\": " << snapshot.edgeSeq << ",\n  \"entities\": [\n";
    for (std::size_t i = 0; i < entities.size(); ++i) {
        const Entity& entity = entities[i];
        entity.validate();
        output << "    {\"id\":" << quoted(entity.id) << ",\"name\":" << quoted(entity.name)
               << ",\"type\":" << quoted(toString(entity.type))
               << ",\"aliases\":" << stringArray(entity.aliases)
               << ",\"description\":" << quoted(entity.description)
               << ",\"sourceMemoryId\":" << quoted(entity.sourceMemoryId)
               << ",\"memoryIds\":" << stringArray(entity.memoryIds)
               << ",\"createdAt\":" << entity.createdAt << ",\"updatedAt\":" << entity.updatedAt
               << ",\"active\":" << (entity.active ? "true" : "false")
               << ",\"invalidAt\":" << entity.invalidAt
               << ",\"invalidReason\":" << quoted(entity.invalidReason)
               << ",\"priority\":" << entity.priority << "}"
               << (i + 1 == entities.size() ? "\n" : ",\n");
    }
    output << "  ],\n  \"edges\": [\n";
    for (std::size_t i = 0; i < edges.size(); ++i) {
        const Edge& edge = edges[i];
        edge.validate();
        output << "    {\"id\":" << quoted(edge.id)
               << ",\"fromEntityId\":" << quoted(edge.fromEntityId)
               << ",\"toEntityId\":" << quoted(edge.toEntityId)
               << ",\"relation\":" << quoted(edge.relation)
               << ",\"description\":" << quoted(edge.description)
               << ",\"eventTime\":" << edge.eventTime << ",\"createdAt\":" << edge.createdAt
               << ",\"validFrom\":" << edge.validFrom << ",\"validTo\":" << edge.validTo
               << ",\"memoryIds\":" << stringArray(edge.memoryIds)
               << ",\"source\":" << quoted(toString(edge.source))
               << ",\"active\":" << (edge.active ? "true" : "false")
               << ",\"invalidAt\":" << edge.invalidAt
               << ",\"invalidReason\":" << quoted(edge.invalidReason)
               << ",\"confidence\":" << edge.confidence << ",\"priority\":" << edge.priority << "}"
               << (i + 1 == edges.size() ? "\n" : ",\n");
    }
    output << "  ]\n}\n";
    output.close();
    if (!output) throw std::runtime_error("failed while writing file: " + temporary);
    std::remove(path.c_str());
    if (std::rename(temporary.c_str(), path.c_str()) != 0) {
        throw std::runtime_error("cannot replace data file: " + path);
    }
}

void GraphJsonStorage::load(const std::string& path, GraphStore& store) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open file for reading: " + path);
    std::ostringstream text;
    text << input.rdbuf();
    if (!input.good() && !input.eof()) throw std::runtime_error("failed while reading file: " + path);

    const Json root = JsonParser(text.str()).parse();
    if (root.kind != Json::Kind::Object) throw std::runtime_error("graph root must be an object");

    GraphStore::Snapshot snapshot;
    const Json& entities = root.at("entities");
    if (entities.kind != Json::Kind::Array) throw std::runtime_error("entities must be an array");
    for (const auto& value : entities.array) {
        Entity entity = parseEntity(value);
        snapshot.entities.emplace(entity.id, std::move(entity));
    }
    const Json& edges = root.at("edges");
    if (edges.kind != Json::Kind::Array) throw std::runtime_error("edges must be an array");
    for (const auto& value : edges.array) {
        Edge edge = parseEdge(value);
        snapshot.edges.emplace(edge.id, std::move(edge));
    }
    snapshot.entitySeq = static_cast<std::size_t>(root.intOr("entitySeq", snapshot.entities.size()));
    snapshot.edgeSeq = static_cast<std::size_t>(root.intOr("edgeSeq", snapshot.edges.size()));
    store.restore(snapshot);
}

}  // namespace memory
