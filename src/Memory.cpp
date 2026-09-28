#include "memory/Memory.h"

#include <chrono>
#include <stdexcept>
#include <utility>

namespace memory {

std::int64_t unixNow() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

Memory Memory::create(std::string idValue, std::string contentValue, int level,
                      MemoryType memoryType, std::vector<std::string> words) {
    const auto now = unixNow();
    Memory result{std::move(idValue), std::move(contentValue), memoryType, level,
                  now, now, now, std::move(words)};
    result.validate();
    return result;
}

void Memory::validate() const {
    if (id.empty()) throw std::invalid_argument("memory id must not be empty");
    if (content.empty()) throw std::invalid_argument("memory content must not be empty");
    if (importance < 1 || importance > 5) {
        throw std::invalid_argument("memory importance must be between 1 and 5");
    }
}

std::string toString(MemoryType type) {
    switch (type) {
        case MemoryType::Profile: return "profile";
        case MemoryType::Plan: return "plan";
        case MemoryType::Conversation: return "conversation";
        case MemoryType::Event: return "event";
        default: return "other";
    }
}

MemoryType memoryTypeFromString(const std::string& value) {
    if (value == "profile") return MemoryType::Profile;
    if (value == "plan") return MemoryType::Plan;
    if (value == "conversation") return MemoryType::Conversation;
    if (value == "event") return MemoryType::Event;
    if (value == "other") return MemoryType::Other;
    throw std::invalid_argument("unknown memory type: " + value);
}

}  // namespace memory

