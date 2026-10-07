#include "memory/Edge.h"

#include "memory/Memory.h"

#include <stdexcept>
#include <utility>

namespace memory {

std::string toString(EdgeSource source) {
    switch (source) {
        case EdgeSource::User: return "user";
        case EdgeSource::Agent: return "agent";
        case EdgeSource::Extractor: return "extractor";
        case EdgeSource::Cooccurrence: return "cooccurrence";
        default: return "extractor";
    }
}

EdgeSource edgeSourceFromString(const std::string& value) {
    if (value == "user") return EdgeSource::User;
    if (value == "agent") return EdgeSource::Agent;
    if (value == "extractor") return EdgeSource::Extractor;
    if (value == "cooccurrence") return EdgeSource::Cooccurrence;
    throw std::invalid_argument("unknown edge source: " + value);
}

Edge Edge::create(std::string idValue, std::string fromValue, std::string toValue,
                  std::string relationValue, std::string descriptionValue,
                  std::vector<std::string> memoryValues, EdgeSource edgeSource,
                  std::int64_t eventTimeValue) {
    const auto now = unixNow();
    Edge result;
    result.id = std::move(idValue);
    result.fromEntityId = std::move(fromValue);
    result.toEntityId = std::move(toValue);
    result.relation = std::move(relationValue);
    result.description = std::move(descriptionValue);
    result.eventTime = eventTimeValue > 0 ? eventTimeValue : now;
    result.createdAt = now;
    result.validFrom = result.eventTime;
    result.memoryIds = std::move(memoryValues);
    result.source = edgeSource;
    result.validate();
    return result;
}

void Edge::addMemoryId(const std::string& memoryId) {
    if (memoryId.empty()) return;
    for (const auto& item : memoryIds) {
        if (item == memoryId) return;
    }
    memoryIds.push_back(memoryId);
}

bool Edge::hasMemoryId(const std::string& memoryId) const {
    for (const auto& item : memoryIds) {
        if (item == memoryId) return true;
    }
    return false;
}

bool Edge::sameFact(const Edge& other) const {
    return fromEntityId == other.fromEntityId && toEntityId == other.toEntityId &&
           relation == other.relation;
}

void Edge::validate() const {
    if (id.empty()) throw std::invalid_argument("edge id must not be empty");
    if (fromEntityId.empty() || toEntityId.empty()) {
        throw std::invalid_argument("edge endpoints must not be empty");
    }
    if (relation.empty()) throw std::invalid_argument("edge relation must not be empty");
    if (confidence < 0.0 || confidence > 1.0) {
        throw std::invalid_argument("edge confidence must be between 0 and 1");
    }
    if (priority < 1 || priority > 5) {
        throw std::invalid_argument("edge priority must be between 1 and 5");
    }
    if (!active && invalidReason.empty()) {
        throw std::invalid_argument("invalid edge requires an invalid reason");
    }
}

}  // namespace memory
