#include "memory/CircularQueue.h"
#include "memory/Edge.h"
#include "memory/Entity.h"
#include "memory/GraphJsonStorage.h"
#include "memory/GraphMemoryService.h"
#include "memory/GraphStore.h"
#include "memory/JsonStorage.h"
#include "memory/LruCache.h"
#include "memory/MemoryRetriever.h"
#include "memory/MemoryHashTable.h"
#include "memory/MemoryManager.h"
#include "memory/MinHeap.h"
#include "memory/Scoring.h"
#include "memory/TextAnalysis.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {
int assertions = 0;

#define CHECK(condition) do { ++assertions; if (!(condition)) throw std::runtime_error(std::string("CHECK failed: ") + #condition + " at line " + std::to_string(__LINE__)); } while (false)

template <typename F>
void checkThrows(F function) {
    ++assertions;
    try { function(); } catch (const std::exception&) { return; }
    throw std::runtime_error("expected exception was not thrown");
}

void testMemoryValidation() {
    auto item = memory::Memory::create("id", "content", 5);
    CHECK(item.id == "id");
    CHECK(item.importance == 5);
    checkThrows([] { memory::Memory::create("", "content"); });
    checkThrows([] { memory::Memory::create("id", "", 3); });
    checkThrows([] { memory::Memory::create("id", "content", 6); });
}

void testCircularQueue() {
    memory::CircularQueue<int> queue(3);
    CHECK(queue.empty());
    CHECK(!queue.push(1).has_value());
    queue.push(2); queue.push(3);
    CHECK(queue.size() == 3);
    auto evicted = queue.push(4);
    CHECK(evicted && *evicted == 1);
    CHECK(queue.values() == std::vector<int>({2, 3, 4}));
    CHECK(queue.pop() == 2);
    CHECK(queue.front() == 3);
    checkThrows([] { memory::CircularQueue<int> invalid(0); });
}

void testHashTable() {
    memory::MemoryHashTable table(1);  // 单桶强制制造冲突。
    CHECK(table.insert(memory::Memory::create("a", "A")));
    CHECK(table.insert(memory::Memory::create("b", "B")));
    CHECK(!table.insert(memory::Memory::create("a", "duplicate")));
    CHECK(table.find("b") && table.find("b")->content == "B");
    CHECK(table.erase("a"));
    CHECK(!table.erase("missing"));
    CHECK(table.size() == 1);
}

void testLru() {
    memory::LruCache cache(2, 1);
    CHECK(!cache.touch("a").has_value());
    cache.touch("b"); cache.touch("a");
    CHECK(cache.order() == std::vector<std::string>({"a", "b"}));
    auto evicted = cache.touch("c");
    CHECK(evicted && *evicted == "b");
    CHECK(!cache.contains("b"));
    CHECK(cache.erase("a"));
    CHECK(cache.size() == 1);
}

void testManagerAndStorage() {
    const std::string path = "agent_memory_test.json";
    memory::MemoryManager manager(2, 2);
    manager.add(memory::Memory::create("1", "first", 3));
    manager.add(memory::Memory::create("2", "second", 4));
    auto evicted = manager.add(memory::Memory::create("3", "third", 5));
    CHECK(evicted && *evicted == "1");
    CHECK(manager.size() == 3);  // 短期淘汰不删除长期记忆。
    CHECK(manager.recentIds() == std::vector<std::string>({"2", "3"}));
    CHECK(manager.update("2", "second updated", 5, {"key"}));
    CHECK(manager.find("2")->content == "second updated");
    checkThrows([&] { manager.add(memory::Memory::create("2", "duplicate")); });
    manager.save(path);

    memory::MemoryManager loaded(2, 2);
    loaded.load(path);
    CHECK(loaded.size() == 3);
    CHECK(loaded.peek("2") && loaded.peek("2")->keywords.size() == 1);
    CHECK(loaded.remove("1"));
    CHECK(loaded.peek("1") == nullptr);
    std::remove(path.c_str());
}

void testCorruptJson() {
    const std::string path = "agent_memory_bad.json";
    { std::ofstream output(path); output << "{broken"; }
    checkThrows([&] { memory::JsonStorage::load(path); });
    std::remove(path.c_str());
}

bool nearlyEqual(double left, double right, double epsilon = 1e-9) {
    return std::fabs(left - right) < epsilon;
}

memory::Memory makeMemory(const std::string& id, const std::string& content, int importance,
                          std::int64_t createdAt, const memory::TextAnalyzer& analyzer) {
    memory::Memory item;
    item.id = id;
    item.content = content;
    item.type = memory::MemoryType::Other;
    item.importance = importance;
    item.createdAt = createdAt;
    item.updatedAt = createdAt;
    item.lastAccessedAt = createdAt;
    item.keywords = analyzer.tokenize(content);
    return item;
}

void testTokenizer() {
    memory::SimpleTokenizer tokenizer;
    const auto tokens = tokenizer.tokenize("高等数学 calculus 2");
    CHECK(tokens.size() == 9);  // 4 单字 + 3 二元组 + 2 个 ASCII 词
    const std::unordered_set<std::string> unique(tokens.begin(), tokens.end());
    CHECK(unique.count("高") == 1);
    CHECK(unique.count("高等") == 1);
    CHECK(unique.count("数学") == 1);
    CHECK(unique.count("calculus") == 1);
    CHECK(unique.count("2") == 1);
    CHECK(tokenizer.tokenize("   ").empty());

    memory::JaccardRelevance relevance;
    CHECK(nearlyEqual(relevance.relevance({"a", "b"}, {"a", "b"}), 1.0));
    CHECK(nearlyEqual(relevance.relevance({"a"}, {"b"}), 0.0));
    CHECK(nearlyEqual(relevance.relevance({"a", "b"}, {"a", "b", "c", "d"}), 0.5));
}

void testScoring() {
    CHECK(nearlyEqual(memory::MemoryScorer::normalizedImportance(1), 0.0));
    CHECK(nearlyEqual(memory::MemoryScorer::normalizedImportance(3), 0.5));
    CHECK(nearlyEqual(memory::MemoryScorer::normalizedImportance(5), 1.0));
    CHECK(nearlyEqual(memory::MemoryScorer::normalizedImportance(9), 1.0));  // 越界被截断

    const std::int64_t now = 1'700'000'000;
    CHECK(nearlyEqual(memory::MemoryScorer::recency(now, now), 1.0));
    CHECK(nearlyEqual(memory::MemoryScorer::recency(now - 30 * 86400, now), std::exp(-1.0)));
    CHECK(memory::MemoryScorer::recency(0, now) < 0.001);
    CHECK(nearlyEqual(memory::MemoryScorer::recency(now + 86400, now), 1.0));  // 未来时间不惩罚

    memory::ScoringWeights weights;
    CHECK(nearlyEqual(memory::MemoryScorer::combine(1.0, 1.0, 1.0, weights), 1.0));
    CHECK(nearlyEqual(memory::MemoryScorer::combine(0.0, 0.0, 0.0, weights), 0.0));
    CHECK(nearlyEqual(memory::MemoryScorer::combine(1.0, 0.0, 0.0, weights), 0.5));
    checkThrows([] {
        memory::ScoringWeights bad{0.0, 0.0, 0.0};
        bad.validate();
    });
}

void testMinHeap() {
    memory::MinHeap<int> heap;
    for (int value : {5, 3, 8, 1, 9, 2}) heap.push(value);
    CHECK(heap.size() == 6);
    CHECK(heap.top() == 1);
    CHECK(heap.pop() == 1);
    CHECK(heap.pop() == 2);
    CHECK(heap.top() == 3);
    CHECK(heap.size() == 4);
    heap.clear();
    CHECK(heap.empty());
}

void testRetriever() {
    memory::TextAnalyzer analyzer;
    const std::int64_t now = 1'700'000'000;
    std::vector<memory::Memory> items;
    items.push_back(makeMemory("m1", "复习高等数学第二章", 5, now, analyzer));
    items.push_back(makeMemory("m2", "用户喜欢晚上学习", 3, now, analyzer));
    items.push_back(makeMemory("m3", "微积分习题", 2, now - 90 * 86400, analyzer));
    items.push_back(makeMemory("m4", "高等数学", 3, now - 60 * 86400, analyzer));
    items.push_back(makeMemory("m5", "高等数学", 3, now, analyzer));

    memory::MemoryRetriever retriever(analyzer);
    const auto top = retriever.retrieve(items, "高等数学", 10, {}, now);
    CHECK(!top.empty());
    // 完全匹配的新记忆在默认权重下排在最前。
    CHECK(top.front().memory.id == "m5");
    CHECK(nearlyEqual(top.front().relevance, 1.0));
    CHECK(top.front().importance > 0.0);

    // 部分匹配的高重要度记忆仍会进入结果，且相关度大于 0。
    bool foundPartial = false;
    for (const auto& entry : top) {
        if (entry.memory.id == "m1") {
            foundPartial = true;
            CHECK(entry.relevance > 0.0);
            CHECK(entry.relevance < 1.0);
            CHECK(nearlyEqual(entry.importance, 1.0));
            CHECK(nearlyEqual(entry.recency, 1.0));
        }
    }
    CHECK(foundPartial);

    // 相同内容、相同重要度时，新记忆优先。
    const auto recencyRanked = retriever.retrieve(items, "高等数学", 5, {}, now);
    std::size_t m5Index = 0;
    std::size_t m4Index = 0;
    for (std::size_t i = 0; i < recencyRanked.size(); ++i) {
        if (recencyRanked[i].memory.id == "m5") m5Index = i;
        if (recencyRanked[i].memory.id == "m4") m4Index = i;
    }
    CHECK(m5Index < m4Index);

    // 只保留重要度权重时，重要度最高的记忆排在最前。
    memory::ScoringWeights importanceOnly{0.0, 1.0, 0.0};
    CHECK(retriever.retrieve(items, "高等数学", 5, importanceOnly, now).front().memory.id == "m1");

    // k 截断与边界。
    CHECK(retriever.retrieve(items, "高等数学", 1, {}, now).size() == 1);
    CHECK(retriever.retrieve(items, "高等数学", 0, {}, now).empty());
    CHECK(retriever.retrieve({}, "高等数学", 5, {}, now).empty());

    std::vector<memory::Memory> many;
    for (int i = 0; i < 15; ++i) {
        many.push_back(makeMemory("x" + std::to_string(i), "高等数学", 3, now - i * 86400, analyzer));
    }
    CHECK(retriever.retrieve(many, "高等数学", 10, {}, now).size() == 10);

    const auto scores = retriever.retrieve(items, "高等数学", 10, {}, now);
    for (std::size_t i = 1; i < scores.size(); ++i) {
        CHECK(scores[i - 1].score >= scores[i].score);
    }
}

void testManagerTokenizesAndRecalls() {
    memory::MemoryManager manager(5, 5);
    manager.add(memory::Memory::create("plan-001", "下周复习高等数学第二章", 5,
                                       memory::MemoryType::Plan));
    manager.add(memory::Memory::create("profile-001", "用户通常晚上八点开始学习", 4,
                                       memory::MemoryType::Profile));
    manager.add(memory::Memory::create("event-001", "星期五提交程序设计实践作业", 5,
                                       memory::MemoryType::Event));

    // 录入时自动分词并存储。
    CHECK(!manager.peek("plan-001")->keywords.empty());

    const auto recalled = manager.recall("高等数学复习", 2);
    CHECK(recalled.size() == 2);
    CHECK(recalled.front().memory.id == "plan-001");
    CHECK(manager.recall("高等数学", 0).empty());
}

void testEntityResolution() {
    memory::GraphStore store;
    const std::string first = store.resolveEntity("多元积分", memory::EntityType::Concept);
    const std::string duplicate = store.resolveEntity("多元积分", memory::EntityType::Concept);
    CHECK(first == duplicate);
    CHECK(store.entityCount() == 1);

    // 相似名称合并为同一实体，并把新名称存为别名。
    const std::string merged = store.resolveEntity("多元微积分", memory::EntityType::Concept);
    CHECK(merged == first);
    CHECK(store.entityCount() == 1);
    CHECK(store.findEntity(first)->hasAlias("多元微积分"));

    // 别名命中也能消解。
    const std::string aliasOwner = store.resolveEntity("calculus", memory::EntityType::Concept,
                                                       {"calculus", "微积分"});
    const std::string aliasHit = store.resolveEntity("calculus", memory::EntityType::Concept);
    CHECK(aliasOwner == aliasHit);

    // 类型不同则不合并。
    const std::string asFact = store.resolveEntity("微积分", memory::EntityType::Fact);
    CHECK(asFact != aliasOwner);

    // 同名跨 memory 的实体仍会合并，并累积证据 memory。
    memory::Entity wrapped = memory::Entity::create("e-x", "多元积分", memory::EntityType::Concept,
                                                    {}, "描述", "mem-9");
    const std::string mergedId = store.addEntity(wrapped);
    CHECK(mergedId == first);
    CHECK(!store.findEntity(first)->memoryIds.empty());
}

void testGraphEntityCrudAndQuery() {
    memory::GraphStore store;
    memory::Entity a = memory::Entity::create("concept-1", "多元积分",
                                              memory::EntityType::Concept, {"多重积分"},
                                              "多元函数积分", "mem-1");
    memory::Entity b = memory::Entity::create("fact-1", "用户需要重修微积分",
                                              memory::EntityType::Fact, {}, "事实", "mem-2");
    store.addEntity(a);
    store.addEntity(b);
    CHECK(store.entityCount() == 2);
    CHECK(store.activeEntityCount() == 2);

    memory::EntityQuery byType;
    byType.type = memory::EntityType::Concept;
    CHECK(store.queryEntities(byType).size() == 1);

    memory::EntityQuery byAlias;
    byAlias.alias = "多重积分";
    CHECK(store.queryEntities(byAlias).size() == 1);

    memory::EntityQuery byMemory;
    byMemory.memoryId = "mem-2";
    CHECK(store.queryEntities(byMemory).size() == 1);

    memory::EntityQuery byFuzzy;
    byFuzzy.fuzzy = "微积分";
    byFuzzy.fuzzyThreshold = 0.1;
    CHECK(!store.queryEntities(byFuzzy).empty());

    CHECK(store.updateEntity("concept-1", "多元积分", "更新后的描述", {"多重积分", "重积分"}, 5));
    CHECK(store.findEntity("concept-1")->priority == 5);
    CHECK(store.addAlias("concept-1", "累次积分"));
    CHECK(store.findEntity("concept-1")->hasAlias("累次积分"));

    CHECK(store.invalidateEntity("concept-1", "已过时"));
    CHECK(!store.findEntity("concept-1")->active);
    CHECK(store.findEntity("concept-1")->invalidReason == "已过时");
    memory::EntityQuery activeOnly;
    activeOnly.active = true;
    CHECK(store.queryEntities(activeOnly).size() == 1);
    CHECK(store.activeEntityCount() == 1);
    CHECK(!store.invalidateEntity("concept-1", "重复失效"));
    checkThrows([&] { store.invalidateEntity("fact-1", ""); });
}

void testGraphEdgeCrud() {
    memory::GraphStore store;
    store.addEntity(memory::Entity::create("a", "多元积分", memory::EntityType::Concept));
    store.addEntity(memory::Entity::create("b", "重积分", memory::EntityType::Concept));
    store.addEntity(memory::Entity::create("c", "曲线积分", memory::EntityType::Concept));

    const std::string edgeId =
        store.addEdge(memory::Edge::create("edge-a", "a", "b", "包含", "描述", {"mem-1"}));
    CHECK(store.edgeCount() == 1);
    CHECK(store.findEdge(edgeId)->hasMemoryId("mem-1"));

    // 完全重复的边只合并证据，不重复建边。
    const std::string dup =
        store.addEdge(memory::Edge::create("edge-dup", "a", "b", "包含", "", {"mem-2"}));
    CHECK(dup == edgeId);
    CHECK(store.edgeCount() == 1);
    CHECK(store.findEdge(edgeId)->hasMemoryId("mem-1"));
    CHECK(store.findEdge(edgeId)->hasMemoryId("mem-2"));

    // 端点不存在时拒绝建边。
    checkThrows([&] {
        store.addEdge(memory::Edge::create("edge-bad", "a", "missing", "指向"));
    });

    memory::EdgeQuery byRelation;
    byRelation.relation = "包含";
    byRelation.active = true;
    CHECK(store.queryEdges(byRelation).size() == 1);

    memory::EdgeQuery incident;
    incident.fromEntityId = "b";
    incident.undirected = true;
    CHECK(store.queryEdges(incident).size() == 1);

    memory::EdgeQuery byMemory;
    byMemory.memoryId = "mem-2";
    CHECK(store.queryEdges(byMemory).size() == 1);

    // 修改关系：失效旧边，新增新边，保留历史。
    const std::string revised =
        store.updateEdge(edgeId, "用作", "新的描述", {"mem-3"}, 0, 0.9, 4);
    CHECK(!revised.empty());
    CHECK(revised != edgeId);
    CHECK(!store.findEdge(edgeId)->active);
    CHECK(store.findEdge(edgeId)->invalidReason == "superseded");
    CHECK(store.findEdge(revised)->active);
    CHECK(store.findEdge(revised)->relation == "用作");
    CHECK(store.findEdge(revised)->hasMemoryId("mem-3"));
    CHECK(store.edgeCount() == 2);

    CHECK(store.invalidateEdge(revised, "用户删除"));
    CHECK(!store.findEdge(revised)->active);
    CHECK(store.activeEdgeCount() == 0);
    CHECK(store.updateEdge("missing", "x", "", {}, 0, 1.0, 3).empty());
}

void testGraphTransactionRollback() {
    memory::GraphStore store;
    store.addEntity(memory::Entity::create("seed", "微积分", memory::EntityType::Concept));
    const std::size_t entitiesBefore = store.entityCount();
    const std::size_t edgesBefore = store.edgeCount();

    memory::GraphStore::RelationSpec broken;
    broken.targetName = "";  // 非法端点，触发异常
    broken.relation = "包含";
    checkThrows([&] {
        store.upsertEntityWithRelations(
            memory::Entity::create("new", "傅里叶变换", memory::EntityType::Concept), {broken});
    });
    CHECK(store.entityCount() == entitiesBefore);  // 整体回滚，无半成品
    CHECK(store.edgeCount() == edgesBefore);
    CHECK(store.findEntity("new") == nullptr);
}

void testGraphUpsertWithRelations() {
    memory::GraphStore store;
    store.addEntity(memory::Entity::create("calc", "微积分", memory::EntityType::Concept));

    memory::GraphStore::RelationSpec toExisting;
    toExisting.targetName = "微积分";
    toExisting.relation = "涉及";
    toExisting.description = "重修微积分";
    toExisting.memoryIds = {"mem-1"};
    toExisting.source = memory::EdgeSource::User;

    memory::GraphStore::RelationSpec toUnknown;
    toUnknown.targetName = "傅里叶变换";
    toUnknown.targetType = memory::EntityType::Concept;
    toUnknown.relation = "先修";
    toUnknown.memoryIds = {"mem-1"};

    const auto result = store.upsertEntityWithRelations(
        memory::Entity::create("fact", "用户需要重修微积分", memory::EntityType::Fact, {},
                               "学习计划", "mem-1"),
        {toExisting, toUnknown});

    CHECK(result.entityId == "fact");
    CHECK(result.edgeIds.size() == 2);
    CHECK(result.createdEntityIds.size() == 2);          // fact + 占位实体
    CHECK(result.placeholderEntityIds.size() == 1);
    CHECK(store.findEntity(result.placeholderEntityIds.front())->name == "傅里叶变换");
    CHECK(!result.mergedEntityIds.empty());              // 命中已有“微积分”
}

void testGraphTraversal() {
    memory::GraphStore store;
    for (const char* id : {"a", "b", "c", "d"}) {
        store.addEntity(memory::Entity::create(id, std::string("node-") + id,
                                               memory::EntityType::Concept));
    }
    store.addEdge("a", "b", "先修", "", {"m"});
    store.addEdge("b", "c", "先修", "", {"m"});
    store.addEdge("c", "d", "先修", "", {"m"});

    memory::BfsOptions depthOne;
    depthOne.maxDepth = 1;
    const auto level1 = store.bfs("a", depthOne);
    CHECK(level1.size() == 1);
    CHECK(level1.front().entityId == "b");
    CHECK(level1.front().relation == "先修");

    memory::BfsOptions depthTwo;
    depthTwo.maxDepth = 2;
    const auto level2 = store.bfs("a", depthTwo);
    CHECK(level2.size() == 2);
    std::unordered_set<std::string> level2Ids;
    for (const auto& item : level2) level2Ids.insert(item.entityId);
    CHECK(level2Ids.count("b") == 1);
    CHECK(level2Ids.count("c") == 1);

    memory::BfsOptions limited;
    limited.maxDepth = 3;
    limited.limit = 1;
    CHECK(store.bfs("a", limited).size() == 1);
    CHECK(store.bfs("missing", depthTwo).empty());

    memory::DfsOptions dfsOptions;
    dfsOptions.maxDepth = 3;
    dfsOptions.limit = 10;
    const auto paths = store.dfs("a", dfsOptions);
    CHECK(!paths.empty());
    bool foundLongPath = false;
    for (const auto& path : paths) {
        CHECK(path.steps.front().entityId == "a");
        if (path.steps.size() == 4) {
            foundLongPath = path.steps[1].entityId == "b" && path.steps[2].entityId == "c" &&
                            path.steps[3].entityId == "d";
        }
    }
    CHECK(foundLongPath);
    CHECK(paths.front().score >= paths.back().score);
}

void testGraphJsonStorage() {
    const std::string path = "agent_graph_test.json";
    memory::GraphStore store;
    store.addEntity(memory::Entity::create("concept-1", "多元积分",
                                           memory::EntityType::Concept, {"多重积分"},
                                           "描述", "mem-1"));
    store.addEntity(memory::Entity::create("fact-1", "用户需要重修微积分",
                                           memory::EntityType::Fact));
    store.addEdge("fact-1", "concept-1", "涉及", "重修", {"mem-2"},
                  memory::EdgeSource::User, 0, 0.8, 4);
    store.invalidateEntity("fact-1", "已解决");
    memory::GraphJsonStorage::save(path, store);

    memory::GraphStore loaded;
    memory::GraphJsonStorage::load(path, loaded);
    CHECK(loaded.entityCount() == store.entityCount());
    CHECK(loaded.edgeCount() == store.edgeCount());
    CHECK(loaded.findEntity("concept-1") != nullptr);
    CHECK(loaded.findEntity("concept-1")->hasAlias("多重积分"));
    CHECK(!loaded.findEntity("fact-1")->active);
    CHECK(loaded.findEntity("fact-1")->invalidReason == "已解决");
    const auto edges = loaded.edges();
    CHECK(edges.size() == 1);
    CHECK(edges.front().relation == "涉及");
    CHECK(edges.front().source == memory::EdgeSource::User);
    CHECK(edges.front().hasMemoryId("mem-2"));
    CHECK(nearlyEqual(edges.front().confidence, 0.8));
    CHECK(edges.front().priority == 4);
    std::remove(path.c_str());

    const std::string bad = "agent_graph_bad.json";
    { std::ofstream output(bad); output << "{broken"; }
    checkThrows([&] {
        memory::GraphStore target;
        memory::GraphJsonStorage::load(bad, target);
    });
    std::remove(bad.c_str());
}

void testGraphMemoryServiceWorkflow() {
    memory::GraphStore store;
    memory::GraphMemoryService service(store);

    memory::RememberRequest request;
    request.memory = memory::Memory::create(
        "mem-1", "用户需要重修微积分，并希望讲解定理时带上数学史", 4,
        memory::MemoryType::Plan);
    request.entities.push_back({"微积分", memory::EntityType::Concept, {"calculus"}, "", 4});
    request.entities.push_back(
        {"用户需要重修微积分", memory::EntityType::Fact, {}, "", 5});
    request.entities.push_back(
        {"讲解定理时带上数学史", memory::EntityType::Preference, {}, "", 3});
    request.relations.push_back(
        {"用户需要重修微积分", "微积分", "涉及", "重修计划", memory::EdgeSource::User, 4, 1.0, 0});
    request.relations.push_back(
        {"讲解定理时带上数学史", "微积分", "适用于", "教学偏好",
         memory::EdgeSource::User, 3, 1.0, 0});

    const auto written = service.remember(request);
    CHECK(written.memoryId == "mem-1");
    CHECK(written.entityIds.size() == 3);
    CHECK(written.edgeIds.size() == 2);
    CHECK(store.entityCount() == 3);
    CHECK(store.edgeCount() == 2);

    const auto recalled = service.recall("微积分");
    CHECK(!recalled.entities.empty());
    CHECK(!recalled.edges.empty());
    CHECK(!recalled.memoryIds.empty());
    CHECK(recalled.memoryIds.front() == "mem-1");

    memory::RecallOptions dfsOptions;
    dfsOptions.useDfs = true;
    dfsOptions.depth = 2;
    const auto withPaths = service.recall("微积分", dfsOptions);
    CHECK(!withPaths.paths.empty());

    // 更新与冲突：旧边失效，新边生效。
    const std::string oldEdge = written.edgeIds.front();
    const std::string newEdge = service.reviseEdge(oldEdge, "包含", "改为包含关系", {"mem-1"});
    CHECK(!newEdge.empty());
    CHECK(store.findEdge(oldEdge)->invalidReason == "superseded");
    CHECK(store.findEdge(newEdge)->active);

    // 遗忘：逻辑删除，检索不再返回被遗忘的实体。
    CHECK(service.forgetEntity(written.entityIds.front(), "用户撤回"));
    CHECK(!store.findEntity(written.entityIds.front())->active);
    const auto afterForget = service.recall("微积分");
    for (const auto& entity : afterForget.entities) {
        CHECK(entity.id != written.entityIds.front());
    }
}

void testPlaceholderResolution() {
    memory::GraphStore store;
    memory::GraphMemoryService service(store);

    // 关系指向未知实体时自动创建占位实体。
    memory::GraphStore::RelationSpec spec;
    spec.targetName = "傅里叶变换";
    spec.relation = "先修";
    spec.memoryIds = {"mem-p"};
    const auto result = store.upsertEntityWithRelations(
        memory::Entity::create("la", "线性代数", memory::EntityType::Concept), {spec});
    CHECK(result.placeholderEntityIds.size() == 1);
    const std::string placeholderId = result.placeholderEntityIds.front();
    CHECK(store.findEntity(placeholderId)->name == "傅里叶变换");

    // 后续抽取出的真实实体（名称写法不同，因此单独成节点），与占位实体合并。
    const std::string realId =
        store.resolveEntity("Fourier 变换", memory::EntityType::Concept, {"FT"}, "mem-q");
    CHECK(realId != placeholderId);
    CHECK(service.resolvePlaceholder(placeholderId, realId));
    CHECK(!store.findEntity(placeholderId)->active);
    CHECK(store.findEntity(placeholderId)->invalidReason.find("placeholder") != std::string::npos);

    memory::EdgeQuery query;
    query.fromEntityId = realId;
    query.undirected = true;
    query.active = true;
    CHECK(!store.queryEdges(query).empty());
    // 合并后原占位实体的关系被重定向到真实实体。
    CHECK(store.queryEdges(query).front()->toEntityId == realId ||
          store.queryEdges(query).front()->fromEntityId == realId);
}
}  // namespace

int main() {
    try {
        testMemoryValidation();
        testCircularQueue();
        testHashTable();
        testLru();
        testManagerAndStorage();
        testCorruptJson();
        testTokenizer();
        testScoring();
        testMinHeap();
        testRetriever();
        testManagerTokenizesAndRecalls();
        testEntityResolution();
        testGraphEntityCrudAndQuery();
        testGraphEdgeCrud();
        testGraphTransactionRollback();
        testGraphUpsertWithRelations();
        testGraphTraversal();
        testGraphJsonStorage();
        testGraphMemoryServiceWorkflow();
        testPlaceholderResolution();
        std::cout << "All tests passed. Assertions: " << assertions << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
