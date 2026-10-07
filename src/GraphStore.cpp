#include "memory/GraphStore.h"

#include "memory/Memory.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace memory {
namespace {

constexpr double kResolveThreshold = 0.5;   // 名称相似度达到该值视为同一实体
constexpr double kSecondsPerDay = 86400.0;
constexpr double kRecencyScaleDays = 30.0;

double sourceTrust(EdgeSource source) {
    switch (source) {
        case EdgeSource::User: return 1.0;
        case EdgeSource::Agent: return 0.8;
        case EdgeSource::Extractor: return 0.6;
        case EdgeSource::Cooccurrence: return 0.4;
        default: return 0.5;
    }
}

double typeWeight(EntityType type) {
    switch (type) {
        case EntityType::Concept: return 1.0;
        case EntityType::Fact: return 0.9;
        case EntityType::Preference: return 0.8;
        default: return 0.5;
    }
}

// 把 source 的别名、证据、描述等并入 target（实体消解后的合并动作）。
void mergeEntity(Entity& target, const Entity& source) {
    if (!source.name.empty() && source.name != target.name) target.addAlias(source.name);
    for (const auto& alias : source.aliases) target.addAlias(alias);
    for (const auto& memoryId : source.memoryIds) target.addMemoryId(memoryId);
    if (target.description.empty()) target.description = source.description;
    if (target.type == EntityType::Other && source.type != EntityType::Other) {
        target.type = source.type;
    }
    target.priority = std::max(target.priority, source.priority);
    target.updatedAt = unixNow();
}

bool containsId(const std::vector<std::string>& values, const std::string& wanted) {
    for (const auto& value : values) {
        if (value == wanted) return true;
    }
    return false;
}

}  // namespace

GraphStore::GraphStore(TextAnalyzer analyzer) : analyzer_(std::move(analyzer)) {}

std::string GraphStore::nextEntityId() {
    std::string candidate;
    do {
        candidate = "entity-" + std::to_string(++entitySeq_);
    } while (entities_.count(candidate) != 0);
    return candidate;
}

std::string GraphStore::nextEdgeId() {
    std::string candidate;
    do {
        candidate = "edge-" + std::to_string(++edgeSeq_);
    } while (edges_.count(candidate) != 0);
    return candidate;
}

double GraphStore::nameSimilarity(const std::string& left, const std::string& right) const {
    const std::string a = normalizeEntityName(left);
    const std::string b = normalizeEntityName(right);
    if (a.empty() || b.empty()) return 0.0;
    if (a == b) return 1.0;
    double score = analyzer_.relevanceStrategy().relevance(analyzer_.tokenize(a),
                                                           analyzer_.tokenize(b));
    // 包含关系常常意味着同一实体的不同写法（如“微积分”与“高等数学微积分”）。
    if (a.find(b) != std::string::npos || b.find(a) != std::string::npos) {
        score = std::max(score, 0.7);
    }
    return score;
}

const Entity* GraphStore::findResolvable(const Entity& candidate, double* similarity) const {
    const Entity* best = nullptr;
    double bestScore = 0.0;
    for (const auto& entry : entities_) {
        const Entity& other = entry.second;
        if (!other.active) continue;
        // 显式类型不同时不合并，避免“概念”和“事实”被错误归并。
        if (candidate.type != EntityType::Other && other.type != EntityType::Other &&
            candidate.type != other.type) {
            continue;
        }
        double score = 0.0;
        if (normalizeEntityName(candidate.name) == normalizeEntityName(other.name) &&
            !normalizeEntityName(candidate.name).empty()) {
            score = 1.0;
        } else if (other.hasAlias(candidate.name) || candidate.hasAlias(other.name)) {
            score = 1.0;
        } else {
            score = nameSimilarity(candidate.name, other.name);
            for (const auto& alias : candidate.aliases) {
                score = std::max(score, nameSimilarity(alias, other.name));
            }
            for (const auto& alias : other.aliases) {
                score = std::max(score, nameSimilarity(candidate.name, alias));
            }
        }
        if (score > bestScore) {
            bestScore = score;
            best = &other;
        }
    }
    if (bestScore < kResolveThreshold) return nullptr;
    if (similarity) *similarity = bestScore;
    return best;
}

const Entity* GraphStore::findEntityByName(const std::string& name, bool allowFuzzy) const {
    const Entity* fuzzyMatch = nullptr;
    double fuzzyScore = 0.0;
    for (const auto& entry : entities_) {
        const Entity& entity = entry.second;
        if (!entity.active) continue;
        if (entity.hasAlias(name)) return &entity;
        if (allowFuzzy) {
            const double score = nameSimilarity(name, entity.name);
            if (score > fuzzyScore) {
                fuzzyScore = score;
                fuzzyMatch = &entity;
            }
        }
    }
    return fuzzyScore >= kResolveThreshold ? fuzzyMatch : nullptr;
}

std::string GraphStore::addEntity(Entity entity) {
    entity.validate();
    if (auto it = entities_.find(entity.id); it != entities_.end()) {
        mergeEntity(it->second, entity);
        return it->first;
    }
    double similarity = 0.0;
    if (const Entity* existing = findResolvable(entity, &similarity)) {
        const std::string existingId = existing->id;
        mergeEntity(entities_.at(existingId), entity);
        return existingId;
    }
    const std::string id = entity.id;
    entities_.emplace(id, std::move(entity));
    return id;
}

std::string GraphStore::resolveEntity(const std::string& name, EntityType type,
                                      const std::vector<std::string>& aliases,
                                      const std::string& sourceMemoryId) {
    if (name.empty()) throw std::invalid_argument("entity name must not be empty");
    Entity candidate = Entity::create(nextEntityId(), name, type, aliases, {}, sourceMemoryId);
    double similarity = 0.0;
    if (const Entity* existing = findResolvable(candidate, &similarity)) {
        const std::string existingId = existing->id;
        mergeEntity(entities_.at(existingId), candidate);
        return existingId;
    }
    const std::string id = candidate.id;
    entities_.emplace(id, std::move(candidate));
    return id;
}

bool GraphStore::updateEntity(const std::string& id, const std::string& name,
                              const std::string& description,
                              const std::vector<std::string>& aliases, int priority,
                              std::optional<EntityType> type) {
    auto it = entities_.find(id);
    if (it == entities_.end()) return false;
    Entity& entity = it->second;
    if (name.empty()) throw std::invalid_argument("entity name must not be empty");
    if (priority < 1 || priority > 5) {
        throw std::invalid_argument("entity priority must be between 1 and 5");
    }
    entity.name = name;
    entity.description = description;
    entity.aliases = aliases;
    entity.priority = priority;
    if (type) entity.type = *type;
    entity.updatedAt = unixNow();
    return true;
}

bool GraphStore::addAlias(const std::string& id, const std::string& alias) {
    auto it = entities_.find(id);
    if (it == entities_.end()) return false;
    it->second.addAlias(alias);
    it->second.updatedAt = unixNow();
    return true;
}

bool GraphStore::invalidateEntity(const std::string& id, const std::string& reason,
                                  std::int64_t at) {
    auto it = entities_.find(id);
    if (it == entities_.end() || !it->second.active) return false;
    if (reason.empty()) throw std::invalid_argument("invalid reason must not be empty");
    Entity& entity = it->second;
    entity.active = false;
    entity.invalidReason = reason;
    entity.invalidAt = at > 0 ? at : unixNow();
    entity.updatedAt = entity.invalidAt;
    return true;
}

Entity* GraphStore::findEntity(const std::string& id) {
    auto it = entities_.find(id);
    return it == entities_.end() ? nullptr : &it->second;
}

const Entity* GraphStore::findEntity(const std::string& id) const {
    auto it = entities_.find(id);
    return it == entities_.end() ? nullptr : &it->second;
}

std::vector<const Entity*> GraphStore::queryEntities(const EntityQuery& query) const {
    struct Hit {
        const Entity* entity;
        double fuzzy;
    };
    std::vector<Hit> hits;
    for (const auto& entry : entities_) {
        const Entity& entity = entry.second;
        if (!query.id.empty() && entity.id != query.id) continue;
        if (query.type && entity.type != *query.type) continue;
        if (!query.name.empty() &&
            normalizeEntityName(entity.name) != normalizeEntityName(query.name)) {
            continue;
        }
        if (!query.alias.empty() && !entity.hasAlias(query.alias)) continue;
        if (!query.memoryId.empty() && !containsId(entity.memoryIds, query.memoryId) &&
            entity.sourceMemoryId != query.memoryId) {
            continue;
        }
        if (query.createdFrom > 0 && entity.createdAt < query.createdFrom) continue;
        if (query.createdTo > 0 && entity.createdAt > query.createdTo) continue;
        if (query.updatedFrom > 0 && entity.updatedAt < query.updatedFrom) continue;
        if (query.updatedTo > 0 && entity.updatedAt > query.updatedTo) continue;
        if (query.active && entity.active != *query.active) continue;

        double fuzzy = 0.0;
        if (!query.fuzzy.empty()) {
            const auto queryTokens = analyzer_.tokenize(query.fuzzy);
            std::vector<std::string> targetTokens = analyzer_.tokenize(entity.name);
            for (const auto& alias : entity.aliases) {
                const auto aliasTokens = analyzer_.tokenize(alias);
                targetTokens.insert(targetTokens.end(), aliasTokens.begin(), aliasTokens.end());
            }
            const auto descriptionTokens = analyzer_.tokenize(entity.description);
            targetTokens.insert(targetTokens.end(), descriptionTokens.begin(),
                                descriptionTokens.end());
            fuzzy = analyzer_.relevanceStrategy().relevance(queryTokens, targetTokens);
            if (fuzzy < query.fuzzyThreshold) continue;
        }
        hits.push_back({&entity, fuzzy});
    }

    if (!query.fuzzy.empty()) {
        std::sort(hits.begin(), hits.end(), [](const Hit& left, const Hit& right) {
            if (left.fuzzy != right.fuzzy) return left.fuzzy > right.fuzzy;
            return left.entity->id < right.entity->id;
        });
    } else {
        std::sort(hits.begin(), hits.end(), [](const Hit& left, const Hit& right) {
            if (left.entity->priority != right.entity->priority) {
                return left.entity->priority > right.entity->priority;
            }
            if (left.entity->createdAt != right.entity->createdAt) {
                return left.entity->createdAt > right.entity->createdAt;
            }
            return left.entity->id < right.entity->id;
        });
    }
    if (query.limit > 0 && hits.size() > query.limit) hits.resize(query.limit);

    std::vector<const Entity*> result;
    result.reserve(hits.size());
    for (const auto& hit : hits) result.push_back(hit.entity);
    return result;
}

std::string GraphStore::addEdge(Edge edge) {
    if (edge.id.empty()) edge.id = nextEdgeId();
    edge.validate();
    if (entities_.count(edge.fromEntityId) == 0) {
        throw std::invalid_argument("edge from-entity does not exist: " + edge.fromEntityId);
    }
    if (entities_.count(edge.toEntityId) == 0) {
        throw std::invalid_argument("edge to-entity does not exist: " + edge.toEntityId);
    }
    // 完全重复的边合并证据，不重复建边。
    for (auto& entry : edges_) {
        Edge& existing = entry.second;
        if (!existing.active || !existing.sameFact(edge)) continue;
        for (const auto& memoryId : edge.memoryIds) existing.addMemoryId(memoryId);
        if (existing.description.empty()) existing.description = edge.description;
        existing.eventTime = std::max(existing.eventTime, edge.eventTime);
        existing.confidence = std::max(existing.confidence, edge.confidence);
        existing.priority = std::max(existing.priority, edge.priority);
        return existing.id;
    }
    const std::string id = edge.id;
    edges_.emplace(id, std::move(edge));
    return id;
}

std::string GraphStore::addEdge(const std::string& fromEntityId, const std::string& toEntityId,
                                const std::string& relation, const std::string& description,
                                const std::vector<std::string>& memoryIds, EdgeSource source,
                                std::int64_t eventTime, double confidence, int priority) {
    Edge edge = Edge::create(nextEdgeId(), fromEntityId, toEntityId, relation, description,
                             memoryIds, source, eventTime);
    edge.confidence = confidence;
    edge.priority = priority;
    return addEdge(std::move(edge));
}

std::string GraphStore::updateEdge(const std::string& id, const std::string& relation,
                                   const std::string& description,
                                   const std::vector<std::string>& memoryIds,
                                   std::int64_t eventTime, double confidence, int priority) {
    auto it = edges_.find(id);
    if (it == edges_.end() || !it->second.active) return {};
    const Edge old = it->second;  // 备份以便失败时恢复
    invalidateEdge(id, "superseded");
    try {
        Edge replacement = Edge::create(nextEdgeId(), old.fromEntityId, old.toEntityId, relation,
                                        description, memoryIds, old.source, eventTime);
        replacement.confidence = confidence;
        replacement.priority = priority;
        replacement.validate();
        const std::string newId = replacement.id;
        edges_.emplace(newId, std::move(replacement));
        return newId;
    } catch (...) {
        edges_[id] = old;  // 恢复旧边，保证要么全成要么全败
        throw;
    }
}

bool GraphStore::invalidateEdge(const std::string& id, const std::string& reason,
                                std::int64_t at) {
    auto it = edges_.find(id);
    if (it == edges_.end() || !it->second.active) return false;
    if (reason.empty()) throw std::invalid_argument("invalid reason must not be empty");
    Edge& edge = it->second;
    edge.active = false;
    edge.invalidReason = reason;
    edge.invalidAt = at > 0 ? at : unixNow();
    edge.validTo = edge.invalidAt;
    return true;
}

Edge* GraphStore::findEdge(const std::string& id) {
    auto it = edges_.find(id);
    return it == edges_.end() ? nullptr : &it->second;
}

const Edge* GraphStore::findEdge(const std::string& id) const {
    auto it = edges_.find(id);
    return it == edges_.end() ? nullptr : &it->second;
}

std::vector<const Edge*> GraphStore::queryEdges(const EdgeQuery& query) const {
    std::vector<const Edge*> result;
    for (const auto& entry : edges_) {
        const Edge& edge = entry.second;
        if (!query.id.empty() && edge.id != query.id) continue;
        if (query.source && edge.source != *query.source) continue;
        if (query.active && edge.active != *query.active) continue;
        if (!query.relation.empty() && edge.relation != query.relation) continue;
        if (!query.memoryId.empty() && !edge.hasMemoryId(query.memoryId)) continue;
        if (query.timeFrom > 0 && edge.eventTime < query.timeFrom) continue;
        if (query.timeTo > 0 && edge.eventTime > query.timeTo) continue;
        if (!query.fromEntityId.empty()) {
            const bool matches = query.undirected
                                     ? (edge.fromEntityId == query.fromEntityId ||
                                        edge.toEntityId == query.fromEntityId)
                                     : edge.fromEntityId == query.fromEntityId;
            if (!matches) continue;
        }
        if (!query.toEntityId.empty()) {
            const bool matches = query.undirected
                                     ? (edge.fromEntityId == query.toEntityId ||
                                        edge.toEntityId == query.toEntityId)
                                     : edge.toEntityId == query.toEntityId;
            if (!matches) continue;
        }
        result.push_back(&edge);
    }
    std::sort(result.begin(), result.end(), [](const Edge* left, const Edge* right) {
        if (left->eventTime != right->eventTime) return left->eventTime > right->eventTime;
        if (left->createdAt != right->createdAt) return left->createdAt > right->createdAt;
        return left->id < right->id;
    });
    if (query.limit > 0 && result.size() > query.limit) result.resize(query.limit);
    return result;
}

std::vector<const Edge*> GraphStore::incidentEdges(const std::string& entityId, bool undirected,
                                                   bool includeInactive) const {
    std::vector<const Edge*> result;
    for (const auto& entry : edges_) {
        const Edge& edge = entry.second;
        if (!includeInactive && !edge.active) continue;
        const bool touches = edge.fromEntityId == entityId || edge.toEntityId == entityId;
        if (!touches) continue;
        if (!undirected && edge.toEntityId != entityId) continue;  // 有向时只沿出边
        result.push_back(&edge);
    }
    return result;
}

double GraphStore::edgeScore(const Edge& edge, const Entity& entity, const std::string& keyword,
                             const TextAnalyzer& analyzer, std::int64_t now) {
    const std::int64_t stamp = edge.eventTime > 0 ? edge.eventTime : edge.createdAt;
    double recency = 0.0;
    if (stamp > 0) {
        const double ageDays = std::max(0.0, static_cast<double>(now - stamp) / kSecondsPerDay);
        recency = std::exp(-ageDays / kRecencyScaleDays);
    }
    double keywordMatch = 0.0;
    if (!keyword.empty()) {
        std::vector<std::string> targetTokens = analyzer.tokenize(entity.name);
        for (const auto& alias : entity.aliases) {
            const auto aliasTokens = analyzer.tokenize(alias);
            targetTokens.insert(targetTokens.end(), aliasTokens.begin(), aliasTokens.end());
        }
        for (const auto& token : analyzer.tokenize(entity.description)) targetTokens.push_back(token);
        for (const auto& token : analyzer.tokenize(edge.relation)) targetTokens.push_back(token);
        for (const auto& token : analyzer.tokenize(edge.description)) targetTokens.push_back(token);
        keywordMatch = analyzer.relevance(keyword, targetTokens);
    }
    const double trust = sourceTrust(edge.source);
    const double type = typeWeight(entity.type);
    const double priority = static_cast<double>(entity.priority) / 5.0;
    const double trustConfidence = 0.5 + 0.5 * edge.confidence;
    const double score = 0.30 * recency + 0.20 * trust + 0.20 * keywordMatch +
                         0.15 * type + 0.15 * priority;
    return std::min(1.0, std::max(0.0, score * trustConfidence));
}

std::vector<BfsItem> GraphStore::bfs(const std::string& startEntityId,
                                     const BfsOptions& options) const {
    const Entity* start = findEntity(startEntityId);
    if (!start) return {};
    const std::int64_t now = options.now > 0 ? options.now : unixNow();
    const int maxDepth = std::max(0, options.maxDepth);

    std::unordered_set<std::string> visited{startEntityId};
    std::vector<BfsItem> discovered;
    std::vector<std::pair<std::string, int>> frontier{{startEntityId, 0}};

    while (!frontier.empty()) {
        const auto [nodeId, depth] = frontier.front();
        frontier.erase(frontier.begin());
        if (depth >= maxDepth) continue;

        auto neighbors = incidentEdges(nodeId, options.undirected, options.includeInactive);
        std::sort(neighbors.begin(), neighbors.end(), [&](const Edge* left, const Edge* right) {
            const std::string leftOther =
                left->fromEntityId == nodeId ? left->toEntityId : left->fromEntityId;
            const std::string rightOther =
                right->fromEntityId == nodeId ? right->toEntityId : right->fromEntityId;
            const Entity* leftEntity = findEntity(leftOther);
            const Entity* rightEntity = findEntity(rightOther);
            const double leftScore = leftEntity ? edgeScore(*left, *leftEntity, options.keyword,
                                                            analyzer_, now)
                                                : 0.0;
            const double rightScore = rightEntity ? edgeScore(*right, *rightEntity, options.keyword,
                                                              analyzer_, now)
                                                  : 0.0;
            if (leftScore != rightScore) return leftScore > rightScore;
            return left->id < right->id;
        });

        for (const Edge* edge : neighbors) {
            const std::string other =
                edge->fromEntityId == nodeId ? edge->toEntityId : edge->fromEntityId;
            if (visited.count(other) != 0) continue;
            const Entity* entity = findEntity(other);
            if (!entity) continue;
            visited.insert(other);
            BfsItem item;
            item.entityId = other;
            item.viaEdgeId = edge->id;
            item.relation = edge->relation;
            item.depth = depth + 1;
            item.score = edgeScore(*edge, *entity, options.keyword, analyzer_, now);
            discovered.push_back(item);
            frontier.emplace_back(other, depth + 1);
        }
    }

    std::sort(discovered.begin(), discovered.end(), [](const BfsItem& left, const BfsItem& right) {
        if (left.score != right.score) return left.score > right.score;
        if (left.depth != right.depth) return left.depth < right.depth;
        return left.entityId < right.entityId;
    });
    if (options.limit > 0 && discovered.size() > options.limit) discovered.resize(options.limit);
    return discovered;
}

std::vector<TraversalPath> GraphStore::dfs(const std::string& startEntityId,
                                           const DfsOptions& options) const {
    if (!findEntity(startEntityId)) return {};
    const std::int64_t now = options.now > 0 ? options.now : unixNow();
    const int maxDepth = std::max(0, options.maxDepth);
    const std::size_t maxPaths =
        std::max<std::size_t>(static_cast<std::size_t>(std::max(1, maxDepth)) * 200, 500);

    std::vector<TraversalPath> paths;
    std::vector<TraversalStep> current;
    std::vector<double> stepScores;
    std::unordered_set<std::string> onPath;

    std::function<void(const std::string&, int)> visit = [&](const std::string& nodeId, int depth) {
        if (depth >= 1) {
            TraversalPath path;
            path.steps = current;
            for (double value : stepScores) path.score += value;
            paths.push_back(std::move(path));
            if (paths.size() >= maxPaths) return;
        }
        if (depth >= maxDepth) return;

        auto neighbors = incidentEdges(nodeId, options.undirected, options.includeInactive);
        std::sort(neighbors.begin(), neighbors.end(), [&](const Edge* left, const Edge* right) {
            const std::string leftOther =
                left->fromEntityId == nodeId ? left->toEntityId : left->fromEntityId;
            const std::string rightOther =
                right->fromEntityId == nodeId ? right->toEntityId : right->fromEntityId;
            const Entity* leftEntity = findEntity(leftOther);
            const Entity* rightEntity = findEntity(rightOther);
            const double leftScore = leftEntity ? edgeScore(*left, *leftEntity, options.keyword,
                                                            analyzer_, now)
                                                : 0.0;
            const double rightScore = rightEntity ? edgeScore(*right, *rightEntity, options.keyword,
                                                              analyzer_, now)
                                                  : 0.0;
            if (leftScore != rightScore) return leftScore > rightScore;
            return left->id < right->id;
        });

        for (const Edge* edge : neighbors) {
            if (paths.size() >= maxPaths) return;
            const std::string other =
                edge->fromEntityId == nodeId ? edge->toEntityId : edge->fromEntityId;
            if (onPath.count(other) != 0) continue;
            const Entity* entity = findEntity(other);
            if (!entity) continue;
            current.push_back({other, edge->id, edge->relation, depth + 1});
            stepScores.push_back(edgeScore(*edge, *entity, options.keyword, analyzer_, now));
            onPath.insert(other);
            visit(other, depth + 1);
            onPath.erase(other);
            stepScores.pop_back();
            current.pop_back();
        }
    };

    current.push_back({startEntityId, {}, {}, 0});
    onPath.insert(startEntityId);
    visit(startEntityId, 0);
    onPath.erase(startEntityId);
    current.pop_back();

    std::sort(paths.begin(), paths.end(), [](const TraversalPath& left, const TraversalPath& right) {
        if (left.score != right.score) return left.score > right.score;
        if (left.steps.size() != right.steps.size()) return left.steps.size() > right.steps.size();
        std::string leftKey;
        std::string rightKey;
        for (const auto& step : left.steps) leftKey += step.entityId + ">";
        for (const auto& step : right.steps) rightKey += step.entityId + ">";
        return leftKey < rightKey;
    });
    if (options.limit > 0 && paths.size() > options.limit) paths.resize(options.limit);
    return paths;
}

GraphStore::UpsertResult GraphStore::upsertEntityWithRelations(
    Entity entity, const std::vector<RelationSpec>& relations) {
    entity.validate();
    GraphTransaction transaction(*this);
    UpsertResult result;

    std::unordered_set<std::string> previous;
    previous.reserve(transaction.initial().entities.size());
    for (const auto& entry : transaction.initial().entities) previous.insert(entry.first);

    double similarity = 0.0;
    const Entity* existing = entities_.count(entity.id) != 0
                                 ? &entities_.at(entity.id)
                                 : findResolvable(entity, &similarity);
    if (existing != nullptr) {
        result.entityId = existing->id;
        result.mergedEntityIds.push_back(existing->id);
        mergeEntity(entities_.at(result.entityId), entity);
    } else {
        result.entityId = entity.id;
        entities_.emplace(result.entityId, std::move(entity));
        result.createdEntityIds.push_back(result.entityId);
    }

    for (const auto& relation : relations) {
        std::string targetId;
        if (const Entity* target = findEntityByName(relation.targetName, true)) {
            targetId = target->id;
            if (previous.count(targetId) != 0 &&
                std::find(result.mergedEntityIds.begin(), result.mergedEntityIds.end(), targetId) ==
                    result.mergedEntityIds.end()) {
                result.mergedEntityIds.push_back(targetId);
            }
        } else {
            // 目标不存在时创建占位实体，后续可再消解合并。
            Entity placeholder = Entity::create(nextEntityId(), relation.targetName,
                                                relation.targetType, {}, "placeholder");
            targetId = placeholder.id;
            entities_.emplace(targetId, std::move(placeholder));
            result.createdEntityIds.push_back(targetId);
            result.placeholderEntityIds.push_back(targetId);
        }
        std::vector<std::string> evidence = relation.memoryIds;
        if (evidence.empty() && !entities_.at(result.entityId).memoryIds.empty()) {
            evidence = entities_.at(result.entityId).memoryIds;
        }
        Edge edge = Edge::create(nextEdgeId(), result.entityId, targetId, relation.relation,
                                 relation.description, evidence, relation.source,
                                 relation.eventTime);
        edge.priority = relation.priority;
        edge.confidence = relation.confidence;
        result.edgeIds.push_back(addEdge(std::move(edge)));
    }

    transaction.commit();
    return result;
}

std::vector<Entity> GraphStore::entities() const {
    std::vector<Entity> result;
    result.reserve(entities_.size());
    for (const auto& entry : entities_) result.push_back(entry.second);
    std::sort(result.begin(), result.end(),
              [](const Entity& left, const Entity& right) { return left.id < right.id; });
    return result;
}

std::vector<Edge> GraphStore::edges() const {
    std::vector<Edge> result;
    result.reserve(edges_.size());
    for (const auto& entry : edges_) result.push_back(entry.second);
    std::sort(result.begin(), result.end(),
              [](const Edge& left, const Edge& right) { return left.id < right.id; });
    return result;
}

std::size_t GraphStore::activeEntityCount() const noexcept {
    std::size_t count = 0;
    for (const auto& entry : entities_) {
        if (entry.second.active) ++count;
    }
    return count;
}

std::size_t GraphStore::activeEdgeCount() const noexcept {
    std::size_t count = 0;
    for (const auto& entry : edges_) {
        if (entry.second.active) ++count;
    }
    return count;
}

void GraphStore::clear() {
    entities_.clear();
    edges_.clear();
    entitySeq_ = 0;
    edgeSeq_ = 0;
}

GraphTransaction::GraphTransaction(GraphStore& store)
    : store_(store), snapshot_(store.capture()) {}

GraphTransaction::~GraphTransaction() {
    if (active_) store_.restore(snapshot_);
}

void GraphTransaction::commit() { active_ = false; }

void GraphTransaction::rollback() {
    if (!active_) return;
    store_.restore(snapshot_);
    active_ = false;
}

}  // namespace memory
