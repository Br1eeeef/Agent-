#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace memory {

// 记忆类型供GUI分类筛选，不影响底层存储结构。
enum class MemoryType { Profile, Plan, Conversation, Event, Other };

struct Memory {
    std::string id;
    std::string content;
    MemoryType type{MemoryType::Other};
    int importance{3};  // 合法范围为1到5。
    std::int64_t createdAt{0};
    std::int64_t updatedAt{0};
    std::int64_t lastAccessedAt{0};
    std::vector<std::string> keywords;

    static Memory create(std::string id, std::string content, int importance = 3,
                         MemoryType type = MemoryType::Other,
                         std::vector<std::string> keywords = {});
    void validate() const;
};

std::string toString(MemoryType type);
MemoryType memoryTypeFromString(const std::string& value);
std::int64_t unixNow();

}  // namespace memory

