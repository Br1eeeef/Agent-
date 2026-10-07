#include "AppContext.h"
#include "core/GraphLayout.h"
#include "models/EdgeTableModel.h"
#include "models/EntityTableModel.h"
#include "models/MemoryTableModel.h"
#include "theme/Theme.h"

#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStringList>
#include <QTextStream>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

QString temporaryDataDir() {
    const QString dir = QDir::temp().filePath(
        QStringLiteral("agent-memory-gui-test-%1").arg(QUuid::createUuid().toString(QUuid::Id128)));
    QDir().mkpath(dir);
    return dir;
}

}  // namespace

// 覆盖界面模型与图布局的核心行为：过滤、排序、幽灵行、刷新联动与布局确定性。
class GuiModelTest : public QObject {
    Q_OBJECT

private slots:
    void memoryModelFilters();
    void memoryModelSorts();
    void memoryModelShowsGhostRows();
    void entityAndEdgeModelsRefresh();
    void graphLayoutIsDeterministicAndBounded();
    void graphLayoutHandlesDegenerateInput();
    void shortTermEvictionIsReported();
    void persistenceRoundTrip();
    void corruptedDataFileIsSurvivable();
};

void GuiModelTest::memoryModelFilters() {
    gui::AppContext context(temporaryDataDir());
    gui::MemoryTableModel model(&context);
    model.refresh();

    const int total = model.rowCount();
    QVERIFY(total >= 5);

    model.setTypeFilter({memory::MemoryType::Plan});
    QVERIFY(model.rowCount() >= 1);
    QVERIFY(model.rowCount() < total);

    model.setTypeFilter({});
    model.setTextFilter(QStringLiteral("高等数学"));
    QVERIFY(model.rowCount() >= 1);
    const QString firstId = model.idAt(0);
    QVERIFY(firstId.startsWith(QStringLiteral("plan-")));

    // 关键词命中：样例数据中 conversation-001 的关键词包含“循环队列”。
    model.setTextFilter(QStringLiteral("循环队列"));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.idAt(0), QStringLiteral("conversation-001"));

    model.setTextFilter(QStringLiteral("不存在的内容"));
    QCOMPARE(model.rowCount(), 0);
}

void GuiModelTest::memoryModelSorts() {
    gui::AppContext context(temporaryDataDir());
    gui::MemoryTableModel model(&context);
    model.setSortMode(gui::MemoryTableModel::ByImportanceDesc);
    model.refresh();

    QVERIFY(model.rowCount() >= 5);
    for (int row = 1; row < model.rowCount(); ++row) {
        QVERIFY(model.importanceAt(row - 1) >= model.importanceAt(row));
    }

    model.setSortMode(gui::MemoryTableModel::ByIdAsc);
    model.refresh();
    QStringList ids;
    for (int row = 0; row < model.rowCount(); ++row) ids << model.idAt(row);
    QStringList sorted = ids;
    std::sort(sorted.begin(), sorted.end());
    QCOMPARE(ids, sorted);
}

void GuiModelTest::memoryModelShowsGhostRows() {
    gui::AppContext context(temporaryDataDir());
    gui::MemoryTableModel model(&context);
    model.refresh();

    QCOMPARE(model.ghostCount(), 0);
    QVERIFY(context.removeMemory(QStringLiteral("event-001"), nullptr));
    model.refresh();

    // 记忆已从长期存储移除，但短期队列仍保留该 ID，因此出现一条幽灵行。
    QCOMPARE(model.ghostCount(), 1);
    const int row = model.rowOfId(QStringLiteral("event-001"));
    QVERIFY(row >= 0);
    QVERIFY(model.isGhost(row));
    QVERIFY(context.manager().peek("event-001") == nullptr);
}

void GuiModelTest::entityAndEdgeModelsRefresh() {
    gui::AppContext context(temporaryDataDir());
    gui::EntityTableModel entityModel(&context);
    gui::EdgeTableModel edgeModel(&context);
    entityModel.refresh();
    edgeModel.refresh();

    const int entities = entityModel.rowCount();
    const int edges = edgeModel.rowCount();
    QVERIFY(entities >= 6);
    QVERIFY(edges >= 5);

    // 关系表里的每一行都要能解析出端点名称，不能出现悬空 ID。
    for (int row = 0; row < edges; ++row) {
        const memory::Edge* edge = edgeModel.edgeAt(row);
        QVERIFY(edge != nullptr);
        QVERIFY(context.graph().findEntity(edge->fromEntityId) != nullptr);
        QVERIFY(context.graph().findEntity(edge->toEntityId) != nullptr);
    }

    // 新增实体后两个模型的可见行数都会增加。
    context.addEntity(memory::Entity::create("ent-test-1", "测试实体",
                                             memory::EntityType::Concept));
    entityModel.refresh();
    QCOMPARE(entityModel.rowCount(), entities + 1);

    // 类型过滤只保留目标类型。
    entityModel.setTypeFilter({memory::EntityType::Concept});
    for (int row = 0; row < entityModel.rowCount(); ++row) {
        const memory::Entity* entity = entityModel.entityAt(row);
        QVERIFY(entity != nullptr);
        // memory::toString(EntityType) 返回 std::string，会干扰 QCOMPARE 的字符串化，改用 QVERIFY。
        QVERIFY(entity->type == memory::EntityType::Concept);
    }

    // 失效实体在“不含失效”时被过滤掉。
    entityModel.setTypeFilter({});
    entityModel.setIncludeInactive(false);
    const int activeOnly = entityModel.rowCount();
    entityModel.setIncludeInactive(true);
    QVERIFY(entityModel.rowCount() >= activeOnly);
}

void GuiModelTest::graphLayoutIsDeterministicAndBounded() {
    std::vector<gui::LayoutNode> nodes;
    for (int i = 0; i < 8; ++i) {
        nodes.push_back({std::string("entity-") + std::to_string(i), 120.0, 32.0});
    }
    std::vector<gui::LayoutEdge> edges = {
        {"entity-0", "entity-1"}, {"entity-1", "entity-2"}, {"entity-2", "entity-3"},
        {"entity-3", "entity-0"}, {"entity-4", "entity-5"},
    };
    const QSizeF area(900.0, 640.0);

    const auto first = gui::GraphLayout::compute(nodes, edges, area, 300);
    const auto second = gui::GraphLayout::compute(nodes, edges, area, 300);
    QCOMPARE(first.size(), nodes.size());
    QCOMPARE(second.size(), nodes.size());

    for (std::size_t i = 0; i < first.size(); ++i) {
        QVERIFY(std::isfinite(first[i].x()));
        QVERIFY(std::isfinite(first[i].y()));
        // 同一份数据两次布局必须完全一致，避免界面打开时节点跳动。
        QCOMPARE(first[i].x(), second[i].x());
        QCOMPARE(first[i].y(), second[i].y());
        // 所有节点都必须落在画布内。
        QVERIFY(first[i].x() >= 0.0 && first[i].x() <= area.width());
        QVERIFY(first[i].y() >= 0.0 && first[i].y() <= area.height());
    }

    // 节点不能全部重合在同一点。
    bool distinct = false;
    for (std::size_t i = 1; i < first.size(); ++i) {
        if (std::hypot(first[i].x() - first[0].x(), first[i].y() - first[0].y()) > 1.0) {
            distinct = true;
            break;
        }
    }
    QVERIFY(distinct);
}

void GuiModelTest::graphLayoutHandlesDegenerateInput() {
    QVERIFY(gui::GraphLayout::compute({}, {}, QSizeF(600, 400)).empty());

    const std::vector<gui::LayoutNode> single = {{"only", 100.0, 32.0}};
    const auto result = gui::GraphLayout::compute(single, {}, QSizeF(600, 400), 50);
    QCOMPARE(result.size(), std::size_t{1});
    QVERIFY(std::isfinite(result[0].x()));
    QVERIFY(std::isfinite(result[0].y()));

    // 边引用了不存在的节点，也不能影响结果或产生 NaN。
    const std::vector<gui::LayoutNode> two = {{"a", 100.0, 32.0}, {"b", 100.0, 32.0}};
    const std::vector<gui::LayoutEdge> dangling = {{"a", "missing"}};
    const auto guarded = gui::GraphLayout::compute(two, dangling, QSizeF(600, 400), 80);
    QCOMPARE(guarded.size(), std::size_t{2});
    for (const auto& point : guarded) {
        QVERIFY(std::isfinite(point.x()));
        QVERIFY(std::isfinite(point.y()));
    }
}

void GuiModelTest::shortTermEvictionIsReported() {
    const QString dir = temporaryDataDir();
    gui::AppContext context(dir);
    gui::MemoryTableModel model(&context);
    // 从空库开始，确保“写满 20 条”这一步本身不会产生淘汰。
    context.clearAll();

    QString evicted;
    QString error;
    for (int i = 1; i <= 20; ++i) {
        const auto value = memory::Memory::create(
            QStringLiteral("fill-%1").arg(i, 2, 10, QLatin1Char('0')).toStdString(),
            QStringLiteral("第 %1 条填充记忆").arg(i).toStdString(), 3,
            memory::MemoryType::Other);
        QVERIFY(context.addMemory(value, &evicted, &error));
        QVERIFY2(evicted.isEmpty(), "容量未满时不应发生淘汰");
    }
    QCOMPARE(context.manager().shortTermSize(), std::size_t{20});

    const auto twentyFirst = memory::Memory::create("fill-21", "第 21 条记忆", 3,
                                                    memory::MemoryType::Other);
    QVERIFY(context.addMemory(twentyFirst, &evicted, &error));
    QCOMPARE(evicted, QStringLiteral("fill-01"));
    // 被淘汰的只是短期队列位置，长期记忆必须保留。
    QVERIFY(context.manager().peek("fill-01") != nullptr);

    model.refresh();
    QVERIFY(model.rowOfId(QStringLiteral("fill-21")) >= 0);
    QVERIFY(model.rowOfId(QStringLiteral("fill-01")) >= 0);
    // 21 条长期记忆都在，且没有任何一条是幽灵行（幽灵行只来自被删除的 ID）。
    QCOMPARE(model.rowCount(), 21);
    QCOMPARE(model.ghostCount(), 0);
}

void GuiModelTest::persistenceRoundTrip() {
    const QString dir = temporaryDataDir();
    QString error;
    int memoryCount = 0;
    std::size_t entityCount = 0;
    std::size_t edgeCount = 0;

    {
        gui::AppContext context(dir);
        QVERIFY(context.addMemory(memory::Memory::create("mem-extra", "新增一条用于持久化的记忆", 4),
                                  nullptr, &error));
        memoryCount = static_cast<int>(context.manager().size());
        entityCount = context.graph().entityCount();
        edgeCount = context.graph().edgeCount();
        QVERIFY(context.saveAll(&error));
        QVERIFY(QFileInfo::exists(context.memoriesPath()));
        QVERIFY(QFileInfo::exists(context.graphPath()));
    }

    gui::AppContext reloaded(dir);
    QCOMPARE(static_cast<int>(reloaded.manager().size()), memoryCount);
    QCOMPARE(reloaded.graph().entityCount(), entityCount);
    QCOMPARE(reloaded.graph().edgeCount(), edgeCount);
    QVERIFY(reloaded.manager().peek("mem-extra") != nullptr);
    QVERIFY(reloaded.manager().peek("plan-001") != nullptr);
}

void GuiModelTest::corruptedDataFileIsSurvivable() {
    const QString dir = temporaryDataDir();
    QString error;
    {
        gui::AppContext context(dir);
        QVERIFY(context.saveAll(&error));
        const QString path = context.memoriesPath();
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write("{ this is not valid json ");
        file.close();
    }

    // 损坏的 JSON 不能让界面崩溃：加载失败要有日志记录，并提供可用的兜底数据。
    gui::AppContext broken(dir);
    QVERIFY(broken.manager().size() >= 1);
    bool hasFailure = false;
    for (const auto& entry : broken.logger().entries()) {
        if (!entry.ok) hasFailure = true;
    }
    QVERIFY2(hasFailure, "损坏的 JSON 加载失败必须写入错误日志");
}

// 这些用例只覆盖模型与布局算法，不创建控件，因此用 QCoreApplication 即可，
// 既不依赖图形平台插件，也能在无桌面环境下稳定运行。
int main(int argc, char* argv[]) {
    QCoreApplication application(argc, argv);
    GuiModelTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_gui_main.moc"
