#include "memory/GraphJsonStorage.h"
#include "memory/GraphMemoryService.h"
#include "memory/GraphStore.h"
#include "memory/MemoryManager.h"

#include <iostream>

namespace {

void demonstrateTopKRecall() {
    std::cout << "== 最小堆 Top-K 检索 ==\n";
    memory::MemoryManager manager(20, 50);
    manager.add(memory::Memory::create("plan-001", "下周复习高等数学第二章", 5,
                                       memory::MemoryType::Plan));
    manager.add(memory::Memory::create("profile-001", "用户通常晚上八点开始学习", 4,
                                       memory::MemoryType::Profile));
    manager.add(memory::Memory::create("event-001", "星期五提交程序设计实践作业", 5,
                                       memory::MemoryType::Event));
    manager.add(memory::Memory::create("conversation-001", "用户询问过多元积分的计算顺序", 3,
                                       memory::MemoryType::Conversation));

    const std::string input = "复习高等数学";
    const auto hits = manager.recall(input, 3);
    std::cout << "输入: " << input << "，返回 " << hits.size() << " 条\n";
    for (const auto& hit : hits) {
        std::cout << "  " << hit.memory.id << " score=" << hit.score
                  << " (relevance=" << hit.relevance << " importance=" << hit.importance
                  << " recency=" << hit.recency << ")\n";
    }
    manager.save("memories.json");
    std::cout << "记忆已保存到 memories.json\n\n";
}

void demonstrateGraph() {
    std::cout << "== 实体关系图数据库 ==\n";
    memory::GraphStore graph;
    memory::GraphMemoryService service(graph);

    memory::RememberRequest request;
    request.memory = memory::Memory::create(
        "mem-101", "用户需要重修微积分，并希望讲解定理时带上数学史", 5,
        memory::MemoryType::Plan);
    request.entities = {
        {"微积分", memory::EntityType::Concept, {"calculus"}, "高等数学基础课程", 5},
        {"用户需要重修微积分", memory::EntityType::Fact, {}, "学习计划", 5},
        {"讲解定理时带上数学史", memory::EntityType::Preference, {}, "教学偏好", 4},
    };
    request.relations = {
        {"用户需要重修微积分", "微积分", "涉及", "重修计划", memory::EdgeSource::User, 5, 1.0, 0},
        {"讲解定理时带上数学史", "微积分", "适用于", "教学偏好",
         memory::EdgeSource::User, 4, 1.0, 0},
    };

    const auto written = service.remember(request);
    std::cout << "写入 memory=" << written.memoryId << "，实体 " << written.entityIds.size()
              << " 个，关系 " << written.edgeIds.size() << " 条\n";

    const auto recalled = service.recall("微积分");
    std::cout << "检索 \"微积分\"：实体 " << recalled.entities.size() << " 个，关系 "
              << recalled.edges.size() << " 条，证据 memory "
              << recalled.memoryIds.size() << " 条\n";
    for (const auto& entity : recalled.entities) {
        std::cout << "  [" << memory::toString(entity.type) << "] " << entity.name << '\n';
    }

    memory::BfsOptions bfsOptions;
    bfsOptions.maxDepth = 2;
    bfsOptions.keyword = "微积分";
    std::cout << "BFS 邻域:\n";
    for (const auto& item : graph.bfs(written.entityIds.front(), bfsOptions)) {
        std::cout << "  深度 " << item.depth << " --" << item.relation << "--> " << item.entityId
                  << " (score=" << item.score << ")\n";
    }

    memory::DfsOptions dfsOptions;
    dfsOptions.maxDepth = 3;
    dfsOptions.limit = 5;
    std::cout << "DFS 路径:\n";
    for (const auto& path : graph.dfs(written.entityIds.front(), dfsOptions)) {
        std::cout << "  ";
        for (std::size_t i = 0; i < path.steps.size(); ++i) {
            if (i != 0) std::cout << " -> ";
            std::cout << path.steps[i].entityId;
        }
        std::cout << " (score=" << path.score << ")\n";
    }

    memory::GraphJsonStorage::save("graph.json", graph);
    std::cout << "图已保存到 graph.json\n\n";
}

}  // namespace

int main() {
    try {
        demonstrateTopKRecall();
        demonstrateGraph();
        std::cout << "Demo finished.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}

