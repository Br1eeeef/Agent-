# Agent Memory Database

面向学习助理 Agent 的本地记忆数据库核心模块。项目使用 C++17，实现短期记忆、长期记忆、LRU 淘汰和 JSON 持久化，并提供可供 Qt GUI 调用的 `MemoryManager` API。

## 项目范围

- 不实现大语言模型，也不依赖在线 API。
- Agent 通过 API 写入和读取记忆。
- 运行时数据保存在内存数据结构中，退出时保存为本地 JSON。
- 默认短期记忆容量为 20，LRU 活跃索引容量为 50。
- Top K 召回由最小堆模块实现，默认 K=10，权重为相关度 0.5、重要度 0.3、时间新鲜度 0.2。
- 实体关系图数据库存储长期记忆中的概念、事实、偏好及其关系，支持 BFS/DFS 扩展。

## 史麓源负责模块

- `Memory` 数据模型与字段校验
- `CircularQueue` 固定容量循环队列
- `MemoryHashTable` 拉链法自定义哈希表
- `LruCache` 双向链表加自定义桶索引
- `JsonStorage` 本地 JSON 保存、加载及异常检查
- `MemoryManager` 统一业务 API
- 单元测试和命令行演示

## 王鹏负责模块

- `MinHeap` 通用二叉最小堆
- `Scoring` 实现记忆评分，相似度采取分词方案
- `GraphStore` 管理 entity/edge 的增删改查、实体消解、事务写入与 BFS/DFS。
- GUI界面

## 构建

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Qt 图形界面（演示用）

`agent_memory_gui` 是覆盖数据库主要功能的 Qt Widgets 演示界面：手动录入记忆、
最小堆 Top-K 召回、实体关系图（力导向画布 + BFS/DFS）、循环队列/LRU/哈希表的
状态可视化以及操作日志。**不接入大模型，也不接入 Agent**，实体与关系全部手工填写。

### 一条命令流程（在项目根目录执行）

下面这段在项目根目录 `Agent-` 下整段粘贴运行即可；**`build` 目录不需要事先存在，
它由第一条 `cmake -S . -B build` 自动创建**（`.gitignore` 已忽略该目录）。

环境要求：Qt 6.9.2 的 MinGW 13.1.0 套件与 Qt 自带的 CMake，二者随 Qt 安装包提供，
因此不需要额外安装编译器。注意系统 PATH 里通常没有 `cmake`，所以第一行必须设置 PATH。

```powershell
cd D:\Gocile\works\CourseDesign\Agent-      # ① 进入项目根目录

# ② 让 Qt 自带的 MinGW、CMake 进入 PATH（每次新开终端都要执行一次）
$QtRoot = "D:\program\Qt"
$env:Path = "$QtRoot\6.9.2\mingw_64\bin;$QtRoot\Tools\mingw1310_64\bin;$QtRoot\Tools\CMake_64\bin;$env:Path"

# ③ 配置（自动创建 build 目录）→ 编译 → 运行测试
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release `
      -DCMAKE_PREFIX_PATH="D:/program/Qt/6.9.2/mingw_64" `
      -DCMAKE_CXX_COMPILER="D:/program/Qt/Tools/mingw1310_64/bin/g++.exe" `
      -DCMAKE_MAKE_PROGRAM="D:/program/Qt/Tools/mingw1310_64/bin/mingw32-make.exe"
cmake --build build -j 8

# ④ 把 Qt 运行库复制到 build 目录，之后运行就不需要再配置 PATH
& "$QtRoot\6.9.2\mingw_64\bin\windeployqt.exe" --no-translations .\build\agent_memory_gui.exe

# ⑤ 运行测试（agent_memory_gui_tests 依赖第 ④ 步复制进来的 Qt 运行库）
ctest --test-dir build --output-on-failure

# ⑥ 启动界面
.\build\agent_memory_gui.exe
```

第 ④ 步是必需的：不部署（或没把 `D:\program\Qt\6.9.2\mingw_64\bin` 加进 PATH）时，
界面程序会因为找不到 `Qt6Core.dll` 等运行库而无法启动（报 0xC0000135）。
如果只想编译命令行 demo 与核心测试，第 ④ 步可以跳过。

### 用 Qt Creator 打开（不想敲命令时）

Qt Creator 可以替代第 ②–⑥ 步：`文件 → 打开文件或项目`，选择本目录的 `CMakeLists.txt`，
在 Configure Project 界面勾选 **MinGW 13.1.0 (64-bit)** 套件（不要选 MSVC 2022，
本机没有对应的 VS2022 编译器），然后点左下角绿色的运行按钮。
Qt Creator 会自己创建构建目录（形如 `build-Desktop_Qt_6_9_2_MinGW_64_bit-Release`），
并在启动前处理 Qt 运行库，因此不需要手工执行 `windeployqt`。
运行配置里把可执行文件选为 `agent_memory_gui` 即可看到界面。

无头导出五个页面的截图（用于交付材料，不会写回 `data/`）：

```powershell
.\build\agent_memory_gui.exe --screenshot docs\screenshots
```

数据文件固定在 `data/memories.json`（记忆）与 `data/graph.json`（实体关系图）。
两者不存在时会自动播种示例数据：记忆取自 `data/sample_memories.json`，
图谱使用内置的“重修微积分”示例；示例记忆的时间戳会平移到当前时间，
以便演示新鲜度衰减。

### 演示脚本（答辩用，逐条对应验收点）

1. **总览**：确认四张指标卡与类型/重要度分布；右侧短期队列占用条显示 `5/20`。
2. **记忆管理 → 新增记忆**：连续录入直至第 21 条，界面提示
   “短期队列已淘汰 `<id>`（长期记忆保留）”，列表中被淘汰的 ID 仍然存在。
3. **记忆管理 → 删除**：删除一条记忆后，列表出现灰化删除线的幽灵行，
   说明 `remove` 不会清理短期队列中的残留 ID。
4. **智能检索**：输入“微积分 复习”，观察结果排行与三项得分条；
   调整 K 值与权重滑块，排序随之变化；点击结果卡后左侧 LRU 顺序更新
   （`recall` 不改变 LRU，只有 `find` 才会）。
5. **实体关系**：观察力导向图与图例；单击节点高亮一跳邻域；
   右键“合并占位实体”演示占位实体并入真实实体。
6. **实体关系 → BFS/DFS**：以“微积分”为起点做 BFS 得到邻域表，
   再做 DFS 得到完整推理链 `用户需要重修微积分 --涉及--> 微积分`。
7. **实体关系 → 修改关系**：修改一条关系后旧边以删除线留在历史分区，新边生效。
8. **实体关系 → 设为失效**：实体灰化，关联边转为失效虚线（逻辑删除 + 历史保留）。
9. **高级录入**：一次写入“记忆 + 3 实体 + 2 关系”，回显新建/合并实体数与占位实体数。
10. **系统状态**：查看环形队列示意、LRU 顺序与操作日志（失败操作是红色行）；
    点击 LRU 行的“访问”按钮演示顺序变化。
11. **持久化**：点击“保存到 data/”后关闭程序再启动，记忆与图谱完整恢复；
    手工把 `data/memories.json` 改坏后启动，界面不崩溃且日志中出现红色失败行。

## 核心 API

```cpp
memory::MemoryManager manager(20, 50);
manager.add(memory::Memory::create("m1", "复习高等数学第二章", 4));

const memory::Memory* item = manager.find("m1");
manager.update("m1", "复习高等数学第三章", 5);

// 最小堆 Top-K 召回，返回带分项得分的记忆。
for (const auto& hit : manager.recall("高等数学", 10)) {
    // hit.memory / hit.score / hit.relevance / hit.importance / hit.recency
}
manager.save("data/memories.json");
manager.load("data/memories.json");
```

Qt GUI 只需要持有一个 `MemoryManager` 对象，通过上述接口完成列表刷新、详情展示和数据保存，不直接操作底层节点指针。

## 图数据库 API

```cpp
memory::GraphStore graph;
memory::GraphMemoryService service(graph);

memory::RememberRequest request;
request.memory = memory::Memory::create("mem-1", "用户需要重修微积分", 5);
request.entities = {{"微积分", memory::EntityType::Concept},
                    {"用户需要重修微积分", memory::EntityType::Fact}};
request.relations = {{"用户需要重修微积分", "微积分", "涉及"}};
service.remember(request);

const auto recalled = service.recall("微积分");   // 实体 + 关系 + 证据 memory

memory::DfsOptions dfs;
dfs.maxDepth = 4;
dfs.limit = 10;
const auto paths = graph.dfs("entity-1", dfs);    // 返回完整路径，便于解释推理链

memory::GraphJsonStorage::save("data/graph.json", graph);
```

