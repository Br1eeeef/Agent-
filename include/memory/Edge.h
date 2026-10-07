#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace memory {

// 边的来源，用于检索时评估可信度。
enum class EdgeSource { User, Agent, Extractor, Cooccurrence };

std::string toString(EdgeSource source);
EdgeSource edgeSourceFromString(const std::string& value);

// 实体之间的关系。删除为逻辑删除（active=false），修改关系时建议失效旧边、
// 新增新边，以保留历史。
struct Edge {
    std::string id;
    std::string fromEntityId;
    std::string toEntityId;
    std::string relation;
    std::string description;
    std::int64_t eventTime{0};
    std::int64_t createdAt{0};
    std::int64_t validFrom{0};
    std::int64_t validTo{0};
    std::vector<std::string> memoryIds;  // 来源证据
    EdgeSource source{EdgeSource::Extractor};
    bool active{true};
    std::int64_t invalidAt{0};
    std::string invalidReason;
    double confidence{1.0};  // 0 到 1，无显式权重时用于排序
    int priority{3};         // 1 到 5

    static Edge create(std::string id, std::string fromEntityId, std::string toEntityId,
                       std::string relation, std::string description = {},
                       std::vector<std::string> memoryIds = {},
                       EdgeSource source = EdgeSource::Extractor,
                       std::int64_t eventTime = 0);

    void addMemoryId(const std::string& memoryId);
    bool hasMemoryId(const std::string& memoryId) const;
    // 判断两条边是否为同一事实：from/to/relation 相同（方向敏感）。
    bool sameFact(const Edge& other) const;
    void validate() const;
};

}  // namespace memory
