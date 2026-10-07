#include "memory/Entity.h"

#include "memory/Memory.h"

#include <cctype>
#include <stdexcept>
#include <utility>

namespace memory {

std::string toString(EntityType type) {
    switch (type) {
        case EntityType::Concept: return "concept";
        case EntityType::Fact: return "fact";
        case EntityType::Preference: return "preference";
        default: return "other";
    }
}

EntityType entityTypeFromString(const std::string& value) {
    if (value == "concept") return EntityType::Concept;
    if (value == "fact") return EntityType::Fact;
    if (value == "preference") return EntityType::Preference;
    if (value == "other") return EntityType::Other;
    throw std::invalid_argument("unknown entity type: " + value);
}

Entity Entity::create(std::string idValue, std::string nameValue, EntityType entityType,
                      std::vector<std::string> aliasValues, std::string descriptionValue,
                      std::string sourceMemoryValue) {
    const auto now = unixNow();
    Entity result;
    result.id = std::move(idValue);
    result.name = std::move(nameValue);
    result.type = entityType;
    result.aliases = std::move(aliasValues);
    result.description = std::move(descriptionValue);
    result.sourceMemoryId = std::move(sourceMemoryValue);
    result.createdAt = now;
    result.updatedAt = now;
    if (!result.sourceMemoryId.empty()) result.memoryIds.push_back(result.sourceMemoryId);
    result.validate();
    return result;
}

bool Entity::hasAlias(const std::string& alias) const {
    const std::string wanted = normalizeEntityName(alias);
    if (wanted.empty()) return false;
    if (normalizeEntityName(name) == wanted) return true;
    for (const auto& item : aliases) {
        if (normalizeEntityName(item) == wanted) return true;
    }
    return false;
}

void Entity::addAlias(const std::string& alias) {
    const std::string normalized = normalizeEntityName(alias);
    if (normalized.empty()) return;
    if (normalizeEntityName(name) == normalized) return;
    for (const auto& item : aliases) {
        if (normalizeEntityName(item) == normalized) return;
    }
    aliases.push_back(alias);
}

void Entity::addMemoryId(const std::string& memoryId) {
    if (memoryId.empty()) return;
    for (const auto& item : memoryIds) {
        if (item == memoryId) return;
    }
    memoryIds.push_back(memoryId);
}

void Entity::validate() const {
    if (id.empty()) throw std::invalid_argument("entity id must not be empty");
    if (name.empty()) throw std::invalid_argument("entity name must not be empty");
    if (priority < 1 || priority > 5) {
        throw std::invalid_argument("entity priority must be between 1 and 5");
    }
    if (!active && invalidReason.empty()) {
        throw std::invalid_argument("invalid entity requires an invalid reason");
    }
}

std::string normalizeEntityName(const std::string& name) {
    std::string result;
    result.reserve(name.size());
    for (unsigned char ch : name) {
        if (std::isspace(ch)) continue;
        if (ch < 0x80) {
            switch (ch) {
                case '"': case '\'': case '(': case ')': case '[': case ']':
                case '{': case '}': case ',': case '.': case ';': case ':':
                case '!': case '?': case '-': case '_': case '/': case '\\':
                case '~': case '`': case '*': case '#': case '&':
                    continue;
                default: break;
            }
            result += static_cast<char>(std::tolower(ch));
        } else {
            result += static_cast<char>(ch);
        }
    }
    return result;
}

}  // namespace memory
