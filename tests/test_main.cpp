#include "memory/CircularQueue.h"
#include "memory/JsonStorage.h"
#include "memory/LruCache.h"
#include "memory/MemoryHashTable.h"
#include "memory/MemoryManager.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
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
}  // namespace

int main() {
    try {
        testMemoryValidation();
        testCircularQueue();
        testHashTable();
        testLru();
        testManagerAndStorage();
        testCorruptJson();
        std::cout << "All tests passed. Assertions: " << assertions << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
