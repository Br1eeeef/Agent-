#pragma once

#include "memory/GraphStore.h"

#include <string>

namespace memory {

// 实体关系图的本地 JSON 持久化。先写临时文件再替换，避免写入中断损坏数据。
class GraphJsonStorage {
public:
    static void save(const std::string& path, const GraphStore& store);
    // 完整解析成功后整体替换图中的数据；解析失败时保留原数据。
    static void load(const std::string& path, GraphStore& store);
};

}  // namespace memory
