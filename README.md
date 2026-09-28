# Agent Memory Database

面向学习助理 Agent 的本地记忆数据库核心模块。项目使用 C++17，实现短期记忆、长期记忆、LRU 淘汰和 JSON 持久化，并提供可供 Qt GUI 调用的 `MemoryManager` API。

## 项目范围

- 不实现大语言模型，也不依赖在线 API。
- Agent 通过 API 写入和读取记忆。
- 运行时数据保存在内存数据结构中，退出时保存为本地 JSON。
- 默认短期记忆容量为 20，LRU 活跃索引容量为 50。
- Top K 召回由另一个模块实现，默认 K=5，权重建议为相关度 0.5、重要度 0.3、时间新鲜度 0.2。

## 史麓源负责模块

- `Memory` 数据模型与字段校验
- `CircularQueue` 固定容量循环队列
- `MemoryHashTable` 拉链法自定义哈希表
- `LruCache` 双向链表加自定义桶索引
- `JsonStorage` 本地 JSON 保存、加载及异常检查
- `MemoryManager` 统一业务 API
- 单元测试和命令行演示

## 构建

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## 核心 API

```cpp
memory::MemoryManager manager(20, 50);
manager.add(memory::Memory::create("m1", "复习高等数学第二章", 4));

const memory::Memory* item = manager.find("m1");
manager.update("m1", "复习高等数学第三章", 5);
manager.save("data/memories.json");
manager.load("data/memories.json");
```

Qt GUI 只需要持有一个 `MemoryManager` 对象，通过上述接口完成列表刷新、详情展示和数据保存，不直接操作底层节点指针。

