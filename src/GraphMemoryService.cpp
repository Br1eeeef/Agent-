#include "memory/GraphMemoryService.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace memory {
namespace {

bool edgeExpired(const Edge& edge, std::int64_t now) {
    return edge.validTo > 0 && edge.validTo <= now;
}

}  // namespace

RememberResult GraphMemoryService::remember(const RememberRequest& request) {
    request.memory.validate();
    const std::string memoryId = request.memory.id;

    GraphTransaction transaction(graph_);
    const GraphStore::Snapshot& initial = transaction.initial();

    std::unordered_set<std::string> previous;
    previous.reserve(initial.entities.size());
    for (const auto& entry : initial.entities) previous.insert(entry.first);

    RememberResult result;
    result.memoryId = memoryId;

    // 1. 实体消解：能合并的合并，否则新建。
    std::unordered_set<std::string> knownNew;
    for (const auto& draft : request.entities) {
        const std::string id =
            graph_.resolveEntity(draft.name, draft.type, draft.aliases, memoryId);
        result.entityIds.push_back(id);
        if (previous.count(id) != 0) {
            if (std::find(result.mergedEntityIds.begin(), result.mergedEntityIds.end(), id) ==
                result.mergedEntityIds.end()) {
                result.mergedEntityIds.push_back(id);
            }
        } else if (knownNew.insert(id).second) {
            result.createdEntityIds.push_back(id);
        }
    }

    // 2. 关系抽取：端点不存在时创建占位实体，边重复时合并证据。
    for (const auto& relation : request.relations) {
        if (relation.subject.empty() || relation.object.empty()) {
            throw std::invalid_argument("relation endpoints must not be empty");
        }
        const std::string fromId =
            graph_.resolveEntity(relation.subject, EntityType::Other, {}, memoryId);
        const std::string toId =
            graph_.resolveEntity(relation.object, EntityType::Other, {}, memoryId);
        for (const std::string& id : {fromId, toId}) {
            if (previous.count(id) == 0 && knownNew.insert(id).second) {
                result.createdEntityIds.push_back(id);
                result.placeholderEntityIds.push_back(id);
            }
        }
        result.edgeIds.push_back(graph_.addEdge(fromId, toId, relation.relation,
                                                relation.description, {memoryId},
                                                relation.source, relation.eventTime,
                                                relation.confidence, relation.priority));
    }

    transaction.commit();
    return result;
}

GraphRecall GraphMemoryService::recall(const std::string& query,
                                       const RecallOptions& options) const {
    GraphRecall result;
    if (query.empty()) return result;
    const std::int64_t now = options.now > 0 ? options.now : unixNow();

    // 1. 查询识别实体：模糊匹配名称/别名/描述。
    EntityQuery seedQuery;
    seedQuery.fuzzy = query;
    seedQuery.fuzzyThreshold = options.fuzzyThreshold;
    if (!options.includeInactive) seedQuery.active = true;
    seedQuery.limit = 3;
    const auto seeds = graph_.queryEntities(seedQuery);
    if (seeds.empty()) return result;

    // 2. BFS 扩展并累计得分。
    std::unordered_map<std::string, double> entityScores;
    std::unordered_set<std::string> edgeIds;
    BfsOptions bfsOptions;
    bfsOptions.maxDepth = options.depth;
    bfsOptions.limit = options.limit;
    bfsOptions.includeInactive = options.includeInactive;
    bfsOptions.undirected = true;
    bfsOptions.keyword = query;
    bfsOptions.now = now;
    for (const Entity* seed : seeds) {
        entityScores.emplace(seed->id, 1.0);
        for (const auto& item : graph_.bfs(seed->id, bfsOptions)) {
            auto it = entityScores.find(item.entityId);
            if (it == entityScores.end()) {
                entityScores.emplace(item.entityId, item.score);
            } else {
                it->second = std::max(it->second, item.score);
            }
            edgeIds.insert(item.viaEdgeId);
        }
    }

    // 3. 过滤无效/过期，排序后返回。
    std::vector<std::pair<std::string, double>> ranked(entityScores.begin(), entityScores.end());
    std::sort(ranked.begin(), ranked.end(), [](const auto& left, const auto& right) {
        if (left.second != right.second) return left.second > right.second;
        return left.first < right.first;
    });
    std::unordered_set<std::string> memoryIds;
    for (const auto& entry : ranked) {
        const Entity* entity = graph_.findEntity(entry.first);
        if (entity == nullptr) continue;
        if (!options.includeInactive && !entity->active) continue;
        result.entities.push_back(*entity);
        for (const auto& memoryId : entity->memoryIds) memoryIds.insert(memoryId);
        if (!entity->sourceMemoryId.empty()) memoryIds.insert(entity->sourceMemoryId);
    }
    for (const auto& edgeId : edgeIds) {
        const Edge* edge = graph_.findEdge(edgeId);
        if (edge == nullptr) continue;
        if (!options.includeInactive && (!edge->active || edgeExpired(*edge, now))) continue;
        result.edges.push_back(*edge);
        for (const auto& memoryId : edge->memoryIds) memoryIds.insert(memoryId);
    }
    std::sort(result.edges.begin(), result.edges.end(), [](const Edge& left, const Edge& right) {
        return left.id < right.id;
    });
    result.memoryIds.assign(memoryIds.begin(), memoryIds.end());
    std::sort(result.memoryIds.begin(), result.memoryIds.end());

    // 4. 需要深度追踪时给出完整路径。
    if (options.useDfs) {
        DfsOptions dfsOptions;
        dfsOptions.limit = options.limit;
        dfsOptions.maxDepth = options.depth;
        dfsOptions.includeInactive = options.includeInactive;
        dfsOptions.undirected = true;
        dfsOptions.keyword = query;
        dfsOptions.now = now;
        result.paths = graph_.dfs(seeds.front()->id, dfsOptions);
    }
    return result;
}

std::string GraphMemoryService::reviseEdge(const std::string& edgeId,
                                           const std::string& relation,
                                           const std::string& description,
                                           const std::vector<std::string>& memoryIds,
                                           std::int64_t eventTime, double confidence,
                                           int priority) {
    return graph_.updateEdge(edgeId, relation, description, memoryIds, eventTime, confidence,
                             priority);
}

bool GraphMemoryService::forgetEntity(const std::string& entityId, const std::string& reason) {
    if (!graph_.invalidateEntity(entityId, reason)) return false;
    // 降低关联边的检索优先级（保留证据，不物理删除）。
    EdgeQuery query;
    query.fromEntityId = entityId;
    query.undirected = true;
    query.active = true;
    for (const Edge* edge : graph_.queryEdges(query)) {
        graph_.invalidateEdge(edge->id, "endpoint invalidated");
    }
    return true;
}

bool GraphMemoryService::forgetEdge(const std::string& edgeId, const std::string& reason) {
    return graph_.invalidateEdge(edgeId, reason);
}

bool GraphMemoryService::resolvePlaceholder(const std::string& placeholderId,
                                            const std::string& realEntityId) {
    if (placeholderId == realEntityId) return false;
    const Entity* placeholder = graph_.findEntity(placeholderId);
    const Entity* real = graph_.findEntity(realEntityId);
    if (placeholder == nullptr || real == nullptr) return false;
    if (!placeholder->active || !real->active) return false;

    GraphTransaction transaction(graph_);
    EdgeQuery query;
    query.fromEntityId = placeholderId;
    query.undirected = true;
    query.active = true;
    const std::vector<const Edge*> incident = graph_.queryEdges(query);
    std::vector<Edge> copies;
    copies.reserve(incident.size());
    for (const Edge* edge : incident) copies.push_back(*edge);  // 先拷贝，避免迭代时修改
    for (const Edge& edge : copies) {
        graph_.invalidateEdge(edge.id, "placeholder merged");
        const std::string fromId =
            edge.fromEntityId == placeholderId ? realEntityId : edge.fromEntityId;
        const std::string toId = edge.toEntityId == placeholderId ? realEntityId : edge.toEntityId;
        graph_.addEdge(fromId, toId, edge.relation, edge.description, edge.memoryIds, edge.source,
                       edge.eventTime, edge.confidence, edge.priority);
    }
    graph_.invalidateEntity(placeholderId, "placeholder merged into " + realEntityId);
    transaction.commit();
    return true;
}

}  // namespace memory
