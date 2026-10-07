#pragma once

#include "memory/Edge.h"
#include "memory/Entity.h"
#include "memory/TextAnalysis.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace memory {

// 实体查询条件。所有已设置字段按“与”组合，未设置字段不参与过滤。
struct EntityQuery {
    std::string id;
    std::optional<EntityType> type;
    std::string name;                 // 精确名称（归一化后比较）
    std::string alias;                // 名称或别名匹配
    std::string memoryId;             // 证据 memory 匹配
    std::int64_t createdFrom{0};
    std::int64_t createdTo{0};
    std::int64_t updatedFrom{0};
    std::int64_t updatedTo{0};
    std::optional<bool> active;
    std::string fuzzy;                // 模糊/语义检索关键词
    double fuzzyThreshold{0.15};      // 相似度阈值
    std::size_t limit{0};             // 0 表示不限制
};

// 边查询条件。
struct EdgeQuery {
    std::string id;
    std::string fromEntityId;
    std::string toEntityId;
    std::string relation;
    std::string memoryId;
    std::optional<EdgeSource> source;
    std::optional<bool> active;
    std::int64_t timeFrom{0};         // 过滤 eventTime
    std::int64_t timeTo{0};
    bool undirected{false};           // true 时 from/to 任一匹配即可
    std::size_t limit{0};
};

// BFS：广度召回，用于上下文构建与多跳问答。
struct BfsOptions {
    int maxDepth{2};
    std::size_t limit{20};
    bool includeInactive{false};
    bool undirected{true};
    std::string keyword;              // 排序用的关键词匹配
    std::int64_t now{0};
};

struct BfsItem {
    std::string entityId;
    std::string viaEdgeId;
    std::string relation;
    int depth{0};
    double score{0.0};
};

// DFS：深度追踪，返回完整路径便于解释推理链/因果链。
struct DfsOptions {
    std::size_t limit{10};
    int maxDepth{4};
    bool includeInactive{false};
    bool undirected{true};
    std::string keyword;
    std::int64_t now{0};
};

struct TraversalStep {
    std::string entityId;
    std::string edgeId;
    std::string relation;
    int depth{0};
};

struct TraversalPath {
    std::vector<TraversalStep> steps;
    double score{0.0};
};

// 实体关系图。实体与边都只做逻辑删除，修改关系时保留历史。
class GraphStore {
public:
    struct Snapshot {
        std::unordered_map<std::string, Entity> entities;
        std::unordered_map<std::string, Edge> edges;
        std::size_t entitySeq{0};
        std::size_t edgeSeq{0};
    };

    explicit GraphStore(TextAnalyzer analyzer = TextAnalyzer());

    // ---- 实体 ----
    // 新增实体，若与已有实体消解为同一对象则合并（别名/证据/描述）。
    // 返回被合并或新增的实体 id。
    std::string addEntity(Entity entity);
    // 按名称/类型/别名做实体消解；不存在则创建，返回实体 id。
    std::string resolveEntity(const std::string& name, EntityType type,
                              const std::vector<std::string>& aliases = {},
                              const std::string& sourceMemoryId = {});
    bool updateEntity(const std::string& id, const std::string& name,
                      const std::string& description,
                      const std::vector<std::string>& aliases, int priority,
                      std::optional<EntityType> type = std::nullopt);
    bool addAlias(const std::string& id, const std::string& alias);
    bool invalidateEntity(const std::string& id, const std::string& reason,
                          std::int64_t at = 0);

    Entity* findEntity(const std::string& id);
    const Entity* findEntity(const std::string& id) const;
    std::vector<const Entity*> queryEntities(const EntityQuery& query) const;

    // ---- 边 ----
    // 新增边；若完全重复（from/to/relation 相同）则合并 memory_ids，不重复建边。
    // 返回最终保留的边 id。
    std::string addEdge(Edge edge);
    // 便捷重载：内部生成边 id。
    std::string addEdge(const std::string& fromEntityId, const std::string& toEntityId,
                        const std::string& relation, const std::string& description,
                        const std::vector<std::string>& memoryIds,
                        EdgeSource source = EdgeSource::Extractor, std::int64_t eventTime = 0,
                        double confidence = 1.0, int priority = 3);
    // 修改关系不直接改 from/to/relation，而是失效旧边并新增新边，保留历史。
    // 返回新边 id；旧边不存在时返回空字符串。
    std::string updateEdge(const std::string& id, const std::string& relation,
                           const std::string& description,
                           const std::vector<std::string>& memoryIds,
                           std::int64_t eventTime, double confidence, int priority);
    bool invalidateEdge(const std::string& id, const std::string& reason,
                        std::int64_t at = 0);
    Edge* findEdge(const std::string& id);
    const Edge* findEdge(const std::string& id) const;
    std::vector<const Edge*> queryEdges(const EdgeQuery& query) const;

    // ---- 遍历 ----
    std::vector<BfsItem> bfs(const std::string& startEntityId, const BfsOptions& options) const;
    std::vector<TraversalPath> dfs(const std::string& startEntityId,
                                   const DfsOptions& options) const;

    // ---- 复合写入（事务化）----
    struct RelationSpec {
        std::string targetName;             // 目标实体名或别名
        EntityType targetType{EntityType::Other};
        std::string relation;
        std::string description;
        std::vector<std::string> memoryIds;
        EdgeSource source{EdgeSource::Extractor};
        int priority{3};
        double confidence{1.0};
        std::int64_t eventTime{0};
    };
    struct UpsertResult {
        std::string entityId;
        std::vector<std::string> edgeIds;
        std::vector<std::string> createdEntityIds;      // 新建（非合并）的实体
        std::vector<std::string> placeholderEntityIds;  // 占位实体，后续可合并
        std::vector<std::string> mergedEntityIds;       // 消解合并到的既有实体
    };
    // 新增实体并建立它与其他实体的关系，整个过程在单个事务中完成：
    // 任一步失败则整体回滚，不留下半成品数据。
    UpsertResult upsertEntityWithRelations(Entity entity,
                                           const std::vector<RelationSpec>& relations);

    std::size_t entityCount() const noexcept { return entities_.size(); }
    std::size_t edgeCount() const noexcept { return edges_.size(); }
    std::vector<Entity> entities() const;
    std::vector<Edge> edges() const;
    std::size_t activeEntityCount() const noexcept;
    std::size_t activeEdgeCount() const noexcept;
    void clear();

    Snapshot capture() const { return {entities_, edges_, entitySeq_, edgeSeq_}; }
    void restore(const Snapshot& snapshot) {
        entities_ = snapshot.entities;
        edges_ = snapshot.edges;
        entitySeq_ = snapshot.entitySeq;
        edgeSeq_ = snapshot.edgeSeq;
    }

private:
    friend class GraphTransaction;

    std::string nextEntityId();
    std::string nextEdgeId();
    // 判断候选实体是否与已有实体消解为同一对象。
    const Entity* findResolvable(const Entity& candidate, double* similarity) const;
    double nameSimilarity(const std::string& left, const std::string& right) const;
    // 依据名称找到实体（精确名称/别名优先，其次模糊相似）。
    const Entity* findEntityByName(const std::string& name, bool allowFuzzy) const;
    std::vector<const Edge*> incidentEdges(const std::string& entityId, bool undirected,
                                           bool includeInactive) const;
    static double edgeScore(const Edge& edge, const Entity& entity, const std::string& keyword,
                            const TextAnalyzer& analyzer, std::int64_t now);

    std::unordered_map<std::string, Entity> entities_;
    std::unordered_map<std::string, Edge> edges_;
    std::size_t entitySeq_{0};
    std::size_t edgeSeq_{0};
    TextAnalyzer analyzer_;
};

// 事务守卫：析构时若未提交则回滚到构造时的快照，保证写入的原子性。
class GraphTransaction {
public:
    explicit GraphTransaction(GraphStore& store);
    ~GraphTransaction();
    GraphTransaction(const GraphTransaction&) = delete;
    GraphTransaction& operator=(const GraphTransaction&) = delete;

    void commit();
    void rollback();
    bool active() const noexcept { return active_; }
    const GraphStore::Snapshot& initial() const noexcept { return snapshot_; }

private:
    GraphStore& store_;
    GraphStore::Snapshot snapshot_;
    bool active_{true};
};

}  // namespace memory
