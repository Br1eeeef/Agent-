#include "memory/MemoryManager.h"

#include <iostream>

int main() {
    try {
        memory::MemoryManager manager;
        manager.add(memory::Memory::create(
            "plan-001", "下周复习高等数学第二章", 5,
            memory::MemoryType::Plan, {"高数", "复习"}));
        manager.add(memory::Memory::create(
            "profile-001", "用户通常晚上八点开始学习", 4,
            memory::MemoryType::Profile, {"学习时间", "晚上"}));

        const memory::Memory* item = manager.find("plan-001");
        std::cout << "Loaded memory: " << (item ? item->content : "not found") << '\n';
        std::cout << "Total memories: " << manager.size() << '\n';
        manager.save("memories.json");
        std::cout << "Saved to memories.json\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}

