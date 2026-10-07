#pragma once

#include "memory/Edge.h"
#include "memory/Entity.h"
#include "memory/GraphStore.h"
#include "memory/Memory.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace memory {

// 由 memory 抽取/包装出的实体草稿。
struct EntityDraft {
    std::string name;
    EntityType type{EntityType::Other};
    std::vector<std::string> aliases;
    std::string description;
    int priority{3};
};

// 实体间关系草稿。端点用名称表示，不存在时创建占位实体。
struct RelationDraft {
    std::string subject;      // 起点实体名/别名
    std::string object;       // 终点实体名/别名
    std::string relation;
    std::string description;
    EdgeSource source{EdgeSource::Extractor};
    int priority{3};
    double confidence{1.0};
    std::int64_t eventTime{0};
};

// 一次长期记忆写入：新记忆 + 抽取的实体 + 实体间关系。
struct RememberRequest {
    Memory memory;
    std::vector<EntityDraft> entities;
    std::vector<RelationDraft> relations;
};

struct RememberResult {
    std::string memoryId;
    std::vector<std::string> entityIds;         // 与请求 entities 顺序一致
    std::vector<std::string> edgeIds;
    std::vector<std::string> createdEntityIds;
    std::vector<std::string> mergedEntityIds;    // 消解合并到的既有实体
    std::vector<std::string> placeholderEntityIds;
};

struct RecallOptions {
    std::size_t limit{20};
    int depth{2};
    bool useDfs{false};
    bool includeInactive{false};
    double fuzzyThreshold{0.1};
    std::int64_t now{0};
};

struct GraphRecall {
    std::vector<Entity> entities;
    std::vector<Edge> edges;
    std::vector<std::string> memoryIds;          // 命中的证据 memory
    std::vector<TraversalPath> paths;            // DFS 检索时给出完整路径
};

// 面向学习助理 Agent 的长期记忆门面：写入、检索、更新与遗忘。
// 不实现大模型，实体与关系由 Agent 抽取后通过本类写入图。
class GraphMemoryService {
public:
    explicit GraphMemoryService(GraphStore& graph) : graph_(graph) {}

    // 写入：新 memory -> 实体消解 -> 建立 edge -> 关联 memory_id，全部在一个事务内完成。
    RememberResult remember(const RememberRequest& request);

    // 检索：查询识别实体 -> 图中定位 -> BFS/DFS 扩展 -> 过滤无效/过期 -> 排序。
    GraphRecall recall(const std::string& query, const RecallOptions& options = {}) const;

    // 更新与冲突：不覆盖旧事实，失效旧边并新增新边。
    std::string reviseEdge(const std::string& edgeId, const std::string& relation,
                           const std::string& description,
                           const std::vector<std::string>& memoryIds,
                           std::int64_t eventTime = 0, double confidence = 1.0,
                           int priority = 3);

    // 遗忘：逻辑删除并降低检索优先级，保留历史。
    bool forgetEntity(const std::string& entityId, const std::string& reason);
    bool forgetEdge(const std::string& edgeId, const std::string& reason);

    // 把已被占位实体替换掉的真实实体合并进来（占位实体失效）。
    bool resolvePlaceholder(const std::string& placeholderId, const std::string& realEntityId);

    GraphStore& graph() { return graph_; }
    const GraphStore& graph() const { return graph_; }

private:
    GraphStore& graph_;
};

}  // namespace memory
