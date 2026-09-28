#pragma once

#include "memory/Memory.h"

#include <string>
#include <vector>

namespace memory {

class JsonStorage {
public:
    // 先写临时文件再替换目标文件，减少写入中断造成的数据损坏。
    static void save(const std::string& path, const std::vector<Memory>& memories);
    static std::vector<Memory> load(const std::string& path);
};

}  // namespace memory

