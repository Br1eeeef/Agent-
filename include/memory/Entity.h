#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace memory {

// 实体类型：概念、客观事实、用户偏好。
enum class EntityType { Concept, Fact, Preference, Other };

std::string toString(EntityType type);
EntityType entityTypeFromString(const std::string& value);

// 图节点。实体由 memory 抽取/包装得到，memory 只作为证据来源，不直接成为节点。
// 实体不做物理删除，失效时置 active=false 并记录 invalidAt/invalidReason。
struct Entity {
    std::string id;
    std::string name;
    EntityType type{EntityType::Other};
    std::vector<std::string> aliases;
    std::string description;
    std::string sourceMemoryId;
    std::vector<std::string> memoryIds;  // 关联的证据 memory
    std::int64_t createdAt{0};
    std::int64_t updatedAt{0};
    bool active{true};
    std::int64_t invalidAt{0};
    std::string invalidReason;
    int priority{3};  // 无显式权重时的检索排序依据，范围 1 到 5

    static Entity create(std::string id, std::string name,
                         EntityType type = EntityType::Other,
                         std::vector<std::string> aliases = {},
                         std::string description = {},
                         std::string sourceMemoryId = {});

    bool hasAlias(const std::string& alias) const;
    void addAlias(const std::string& alias);
    void addMemoryId(const std::string& memoryId);
    void validate() const;
};

// 实体消解用的名称归一化：去除空白与常见分隔标点，并统一 ASCII 小写。
std::string normalizeEntityName(const std::string& name);

}  // namespace memory
