#include "pages/GraphPage.h"

#include "Format.h"
#include "dialogs/EntityEditDialog.h"
#include "dialogs/RelationEditDialog.h"
#include "dialogs/RememberDialog.h"
#include "models/EdgeTableModel.h"
#include "models/EntityTableModel.h"
#include "theme/Theme.h"
#include "widgets/Card.h"
#include "widgets/GraphView.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QSpinBox>
#include <QSplitter>
#include <QTableView>
#include <QTabWidget>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <stdexcept>

namespace gui {

GraphPage::GraphPage(AppContext* context, QWidget* parent)
    : QWidget(parent), context_(context) {
    entityModel_ = new EntityTableModel(context, this);
    edgeModel_ = new EdgeTableModel(context, this);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(12);
    root->addWidget(buildToolbar());

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(buildEntityList());
    splitter->addWidget(buildCanvas());
    splitter->addWidget(buildInspector());
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 6);
    splitter->setStretchFactor(2, 4);
    splitter->setChildrenCollapsible(false);
    splitter->setSizes({250, 570, 420});
    root->addWidget(splitter, 1);

    connect(context_, &AppContext::graphChanged, this, &GraphPage::refresh);
    refresh();
}

QWidget* GraphPage::buildToolbar() {
    auto* bar = new QWidget(this);
    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* entityButton = new QPushButton(QStringLiteral("新增实体"), bar);
    entityButton->setProperty("variant", "primary");
    auto* relationButton = new QPushButton(QStringLiteral("新增关系"), bar);
    auto* advancedButton = new QPushButton(QStringLiteral("高级录入（记忆+实体+关系）"), bar);
    auto* relayoutButton = new QPushButton(QStringLiteral("重新布局"), bar);
    auto* saveButton = new QPushButton(QStringLiteral("保存图到 data/graph.json"), bar);
    auto* exportButton = new QPushButton(QStringLiteral("导出画布 PNG"), bar);

    layout->addWidget(entityButton);
    layout->addWidget(relationButton);
    layout->addWidget(advancedButton);
    layout->addStretch(1);
    layout->addWidget(relayoutButton);
    layout->addWidget(saveButton);
    layout->addWidget(exportButton);

    connect(entityButton, &QPushButton::clicked, this, &GraphPage::createEntity);
    connect(relationButton, &QPushButton::clicked, this, &GraphPage::createRelation);
    connect(advancedButton, &QPushButton::clicked, this, &GraphPage::openAdvancedEntry);
    connect(relayoutButton, &QPushButton::clicked, this, [this]() { canvas_->relayout(); });
    connect(exportButton, &QPushButton::clicked, this, &GraphPage::exportImage);
    connect(saveButton, &QPushButton::clicked, this, [this]() {
        QString error;
        if (context_->saveAll(&error)) {
            emit toastRequested(QStringLiteral("记忆与实体关系图已保存"), false);
        } else {
            emit toastRequested(QStringLiteral("保存失败：%1").arg(error), true);
        }
    });
    return bar;
}

QWidget* GraphPage::buildEntityList() {
    auto* body = new QWidget(this);
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    entitySearch_ = new QLineEdit(body);
    entitySearch_->setPlaceholderText(QStringLiteral("搜索名称 / 别名 / 描述"));
    entitySearch_->setClearButtonEnabled(true);
    layout->addWidget(entitySearch_);

    auto* filterRow = new QHBoxLayout();
    entityTypeFilter_ = new QComboBox(body);
    entityTypeFilter_->addItem(QStringLiteral("全部类型"), -1);
    entityTypeFilter_->addItem(theme::entityTypeLabel(memory::EntityType::Concept),
                               static_cast<int>(memory::EntityType::Concept));
    entityTypeFilter_->addItem(theme::entityTypeLabel(memory::EntityType::Fact),
                               static_cast<int>(memory::EntityType::Fact));
    entityTypeFilter_->addItem(theme::entityTypeLabel(memory::EntityType::Preference),
                               static_cast<int>(memory::EntityType::Preference));
    entityTypeFilter_->addItem(theme::entityTypeLabel(memory::EntityType::Other),
                               static_cast<int>(memory::EntityType::Other));
    filterRow->addWidget(entityTypeFilter_, 1);
    showInactiveEntities_ = new QCheckBox(QStringLiteral("含失效"), body);
    showInactiveEntities_->setChecked(true);
    filterRow->addWidget(showInactiveEntities_);
    layout->addLayout(filterRow);

    entityTable_ = new QTableView(body);
    entityTable_->setModel(entityModel_);
    entityTable_->setFrameShape(QFrame::NoFrame);
    entityTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    entityTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    entityTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    entityTable_->verticalHeader()->setVisible(false);
    entityTable_->verticalHeader()->setDefaultSectionSize(32);
    entityTable_->setShowGrid(false);
    entityTable_->horizontalHeader()->setSectionResizeMode(EntityTableModel::NameColumn,
                                                          QHeaderView::Stretch);
    entityTable_->horizontalHeader()->setSectionResizeMode(EntityTableModel::AliasColumn,
                                                          QHeaderView::ResizeToContents);
    layout->addWidget(entityTable_, 1);

    connect(entitySearch_, &QLineEdit::textChanged, this,
            [this](const QString& text) { entityModel_->setTextFilter(text); });
    connect(entityTypeFilter_, &QComboBox::currentIndexChanged, this, [this](int) {
        std::vector<memory::EntityType> types;
        const int value = entityTypeFilter_->currentData().toInt();
        if (value >= 0) types.push_back(static_cast<memory::EntityType>(value));
        entityModel_->setTypeFilter(types);
    });
    connect(showInactiveEntities_, &QCheckBox::toggled, this,
            [this](bool checked) { entityModel_->setIncludeInactive(checked); });
    connect(entityTable_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this]() {
                if (suppressSelection_) return;
                const QString id = selectedEntityId();
                canvas_->selectNode(id);
                updateDetail();
                traversalStart_->setText(id.isEmpty()
                                             ? QStringLiteral("未选择")
                                             : QStringLiteral("起点：%1").arg(id));
                edgeModel_->setEntityScope(id);
            });
    connect(entityTable_, &QTableView::doubleClicked, this, &GraphPage::editSelectedEntity);
    return makeCard(QStringLiteral("实体列表"), body, this);
}

QWidget* GraphPage::buildCanvas() {
    auto* body = new QWidget(this);
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    canvas_ = new GraphView(body);
    layout->addWidget(canvas_, 1);

    canvasHint_ = new QLabel(
        QStringLiteral("滚轮缩放 · 空白拖拽平移 · 拖动节点微调 · 单击高亮一跳邻域 · "
                       "双击查看详情 · 右键更多操作"),
        body);
    canvasHint_->setWordWrap(true);
    canvasHint_->setStyleSheet(QStringLiteral("QLabel { color: %1; }").arg(theme::textMuted()));
    layout->addWidget(canvasHint_);

    connect(canvas_, &GraphView::nodeSelected, this, [this](const QString& id) {
        if (suppressSelection_ || id.isEmpty()) return;
        const int row = entityModel_->rowOfId(id);
        if (row >= 0) {
            suppressSelection_ = true;
            entityTable_->selectRow(row);
            entityTable_->scrollTo(entityModel_->index(row, 0));
            suppressSelection_ = false;
        }
        updateDetail();
        edgeModel_->setEntityScope(id);
        traversalStart_->setText(QStringLiteral("起点：%1").arg(id));
    });
    connect(canvas_, &GraphView::nodeContextMenuRequested, this,
            [this](const QString& id, const QPoint& globalPos) {
                canvas_->selectNode(id);
                const int row = entityModel_->rowOfId(id);
                if (row >= 0) {
                    suppressSelection_ = true;
                    entityTable_->selectRow(row);
                    suppressSelection_ = false;
                }
                updateDetail();
                QMenu menu(this);
                menu.addAction(QStringLiteral("查看详情"), this, [this]() { updateDetail(); });
                menu.addAction(QStringLiteral("编辑实体"), this,
                               [this]() { editSelectedEntity(); });
                menu.addAction(QStringLiteral("以此为起点做 BFS"), this,
                               [this]() { runBfs(); });
                menu.addAction(QStringLiteral("以此为起点做 DFS"), this,
                               [this]() { runDfs(); });
                menu.addSeparator();
                menu.addAction(QStringLiteral("设为失效"), this,
                               [this]() { invalidateSelectedEntity(); });
                menu.addAction(QStringLiteral("合并占位实体"), this,
                               [this]() { mergePlaceholder(); });
                menu.exec(globalPos);
            });
    return makeCard(QStringLiteral("实体关系图"), body, this);
}

QWidget* GraphPage::buildInspector() {
    inspector_ = new QTabWidget(this);
    inspector_->addTab(buildDetailTab(), QStringLiteral("实体详情"));
    inspector_->addTab(buildRelationTab(), QStringLiteral("关系"));
    inspector_->addTab(buildTraversalTab(), QStringLiteral("BFS / DFS"));
    return makeCard(QStringLiteral("检视面板"), inspector_, this);
}

QWidget* GraphPage::buildDetailTab() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 10, 8, 8);
    layout->setSpacing(8);

    detailBrowser_ = new QTextBrowser(page);
    detailBrowser_->setFrameShape(QFrame::NoFrame);
    layout->addWidget(detailBrowser_, 1);

    auto* row = new QHBoxLayout();
    editEntityButton_ = new QPushButton(QStringLiteral("编辑实体"), page);
    invalidateEntityButton_ = new QPushButton(QStringLiteral("设为失效"), page);
    invalidateEntityButton_->setProperty("variant", "danger");
    mergeButton_ = new QPushButton(QStringLiteral("合并占位实体"), page);
    row->addWidget(editEntityButton_);
    row->addWidget(invalidateEntityButton_);
    row->addWidget(mergeButton_);
    layout->addLayout(row);

    connect(editEntityButton_, &QPushButton::clicked, this, &GraphPage::editSelectedEntity);
    connect(invalidateEntityButton_, &QPushButton::clicked, this,
            &GraphPage::invalidateSelectedEntity);
    connect(mergeButton_, &QPushButton::clicked, this, &GraphPage::mergePlaceholder);
    connect(detailBrowser_, &QTextBrowser::anchorClicked, this, [this](const QUrl& url) {
        if (url.scheme() == QStringLiteral("mem")) emit memoryRequested(url.path());
    });
    return page;
}

QWidget* GraphPage::buildRelationTab() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 10, 8, 8);
    layout->setSpacing(8);

    auto* filterRow = new QHBoxLayout();
    showInactiveEdges_ = new QCheckBox(QStringLiteral("显示已失效关系"), page);
    showInactiveEdges_->setChecked(true);
    filterRow->addWidget(showInactiveEdges_);
    filterRow->addStretch(1);
    layout->addLayout(filterRow);

    edgeTable_ = new QTableView(page);
    edgeTable_->setModel(edgeModel_);
    edgeTable_->setFrameShape(QFrame::NoFrame);
    edgeTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    edgeTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    edgeTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    edgeTable_->verticalHeader()->setVisible(false);
    edgeTable_->verticalHeader()->setDefaultSectionSize(30);
    edgeTable_->setShowGrid(false);
    edgeTable_->horizontalHeader()->setSectionResizeMode(EdgeTableModel::FromColumn,
                                                         QHeaderView::ResizeToContents);
    edgeTable_->horizontalHeader()->setSectionResizeMode(EdgeTableModel::RelationColumn,
                                                         QHeaderView::Stretch);
    edgeTable_->horizontalHeader()->setSectionResizeMode(EdgeTableModel::ToColumn,
                                                         QHeaderView::ResizeToContents);
    layout->addWidget(edgeTable_, 1);

    auto* row = new QHBoxLayout();
    reviseEdgeButton_ = new QPushButton(QStringLiteral("修改关系"), page);
    invalidateEdgeButton_ = new QPushButton(QStringLiteral("关系失效"), page);
    invalidateEdgeButton_->setProperty("variant", "danger");
    row->addWidget(reviseEdgeButton_);
    row->addWidget(invalidateEdgeButton_);
    layout->addLayout(row);

    connect(showInactiveEdges_, &QCheckBox::toggled, this,
            [this](bool checked) { edgeModel_->setIncludeInactive(checked); });
    connect(reviseEdgeButton_, &QPushButton::clicked, this, &GraphPage::reviseSelectedEdge);
    connect(invalidateEdgeButton_, &QPushButton::clicked, this,
            &GraphPage::invalidateSelectedEdge);
    return page;
}

QWidget* GraphPage::buildTraversalTab() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 10, 8, 8);
    layout->setSpacing(8);

    traversalStart_ = new QLabel(QStringLiteral("未选择"), page);
    traversalStart_->setStyleSheet(
        QStringLiteral("QLabel { color: %1; font-weight: 600; }").arg(theme::textPrimary()));
    traversalStart_->setWordWrap(true);
    layout->addWidget(traversalStart_);

    auto* form = new QFormLayout();
    form->setSpacing(6);
    depthSpin_ = new QSpinBox(page);
    depthSpin_->setRange(1, 6);
    depthSpin_->setValue(2);
    form->addRow(QStringLiteral("最大深度"), depthSpin_);
    limitSpin_ = new QSpinBox(page);
    limitSpin_->setRange(1, 50);
    limitSpin_->setValue(10);
    form->addRow(QStringLiteral("返回条数"), limitSpin_);
    keywordEdit_ = new QLineEdit(page);
    keywordEdit_->setPlaceholderText(QStringLiteral("排序用关键词，可留空"));
    form->addRow(QStringLiteral("关键词"), keywordEdit_);
    undirectedCheck_ = new QCheckBox(QStringLiteral("按无向图遍历"), page);
    undirectedCheck_->setChecked(true);
    form->addRow(QString(), undirectedCheck_);
    layout->addLayout(form);

    auto* row = new QHBoxLayout();
    auto* bfsButton = new QPushButton(QStringLiteral("BFS 邻域"), page);
    bfsButton->setProperty("variant", "primary");
    auto* dfsButton = new QPushButton(QStringLiteral("DFS 推理链"), page);
    row->addWidget(bfsButton);
    row->addWidget(dfsButton);
    layout->addLayout(row);

    traversalList_ = new QListWidget(page);
    traversalList_->setFrameShape(QFrame::NoFrame);
    layout->addWidget(traversalList_, 1);

    connect(bfsButton, &QPushButton::clicked, this, &GraphPage::runBfs);
    connect(dfsButton, &QPushButton::clicked, this, &GraphPage::runDfs);
    return page;
}

QString GraphPage::selectedEntityId() const {
    const QModelIndexList rows = entityTable_->selectionModel()->selectedRows();
    if (rows.isEmpty()) return {};
    return entityModel_->idAt(rows.first().row());
}

QString GraphPage::selectedEdgeId() const {
    const QModelIndexList rows = edgeTable_->selectionModel()->selectedRows();
    if (rows.isEmpty()) return {};
    return edgeModel_->idAt(rows.first().row());
}

void GraphPage::refresh() {
    const QString keep = selectedEntityId();
    entityModel_->refresh();
    edgeModel_->refresh();
    if (!keep.isEmpty()) {
        const int row = entityModel_->rowOfId(keep);
        if (row >= 0) {
            suppressSelection_ = true;
            entityTable_->selectRow(row);
            suppressSelection_ = false;
        }
    }
    rebuildGraph();
    updateDetail();
}

void GraphPage::rebuildGraph() {
    QVector<GraphNodeData> nodes;
    QSet<QString> visibleIds;
    const bool showInactive = showInactiveEntities_->isChecked();
    for (const auto& entity : context_->graph().entities()) {
        if (!showInactive && !entity.active) continue;
        GraphNodeData node;
        node.id = QString::fromStdString(entity.id);
        node.label = QString::fromStdString(entity.name);
        node.color = theme::entityTypeColor(entity.type);
        node.active = entity.active;
        nodes.push_back(node);
        visibleIds.insert(node.id);
    }

    QVector<GraphEdgeData> edges;
    const bool showInactiveEdge = showInactiveEdges_->isChecked();
    for (const auto& edge : context_->graph().edges()) {
        const QString from = QString::fromStdString(edge.fromEntityId);
        const QString to = QString::fromStdString(edge.toEntityId);
        if (!visibleIds.contains(from) || !visibleIds.contains(to)) continue;
        if (!showInactiveEdge && !edge.active) continue;
        GraphEdgeData data;
        data.id = QString::fromStdString(edge.id);
        data.from = from;
        data.to = to;
        data.relation = QString::fromStdString(edge.relation);
        data.active = edge.active;
        data.confidence = edge.confidence;
        data.priority = edge.priority;
        edges.push_back(data);
    }

    const QString center = selectedEntityId();
    if (nodes.size() > 150 && !center.isEmpty()) {
        QSet<QString> keep;
        keep.insert(center);
        for (const auto& edge : edges) {
            if (edge.from == center) keep.insert(edge.to);
            if (edge.to == center) keep.insert(edge.from);
        }
        QVector<GraphNodeData> filteredNodes;
        for (const auto& node : nodes) {
            if (keep.contains(node.id)) filteredNodes.push_back(node);
        }
        QVector<GraphEdgeData> filteredEdges;
        for (const auto& edge : edges) {
            if (keep.contains(edge.from) && keep.contains(edge.to)) filteredEdges.push_back(edge);
        }
        nodes = filteredNodes;
        edges = filteredEdges;
        canvasHint_->setText(
            QStringLiteral("节点超过 150 个，已自动只显示选中实体的一跳邻域。"));
    } else {
        canvasHint_->setText(
            QStringLiteral("滚轮缩放 · 空白拖拽平移 · 拖动节点微调 · 单击高亮一跳邻域 · "
                           "双击查看详情 · 右键更多操作"));
    }

    canvas_->setGraph(nodes, edges);
    if (!center.isEmpty()) canvas_->selectNode(center);
}

void GraphPage::focusEntity(const QString& entityId) {
    const int row = entityModel_->rowOfId(entityId);
    if (row < 0) return;
    entityTable_->selectRow(row);
    canvas_->selectNode(entityId);
    updateDetail();
}

void GraphPage::updateDetail() {
    const QString id = selectedEntityId();
    const memory::Entity* entity =
        id.isEmpty() ? nullptr : context_->graph().findEntity(id.toStdString());

    const bool placeholder =
        entity != nullptr && entityModel_->isPlaceholder(entityModel_->rowOfId(id));
    editEntityButton_->setEnabled(entity != nullptr);
    invalidateEntityButton_->setEnabled(entity != nullptr && entity->active);
    mergeButton_->setEnabled(placeholder);

    if (entity == nullptr) {
        detailBrowser_->setHtml(
            QStringLiteral("<div style='color:%1;'>在左侧或画布中选择一个实体查看详情。</div>")
                .arg(theme::textMuted()));
        return;
    }

    QStringList aliases;
    for (const auto& alias : entity->aliases) aliases << QString::fromStdString(alias);
    QStringList memories;
    for (const auto& memoryId : entity->memoryIds) {
        memories << QStringLiteral("<a href='mem:%1'>%1</a>")
                        .arg(QString::fromStdString(memoryId));
    }
    if (!entity->sourceMemoryId.empty() &&
        std::find(entity->memoryIds.begin(), entity->memoryIds.end(),
                  entity->sourceMemoryId) == entity->memoryIds.end()) {
        memories << QStringLiteral("<a href='mem:%1'>%1</a>")
                        .arg(QString::fromStdString(entity->sourceMemoryId));
    }

    const QString status = entity->active
                               ? QStringLiteral("活跃")
                               : QStringLiteral("已失效：%1").arg(
                                     QString::fromStdString(entity->invalidReason));
    detailBrowser_->setHtml(
        QStringLiteral("<h3 style='color:%1; margin:0 0 6px 0;'>%2</h3>"
                       "<p style='color:%3; margin:0 0 10px 0;'>%4</p>"
                       "<table cellspacing='0' cellpadding='3' style='color:%3;'>"
                       "<tr><td>实体ID</td><td><b>%5</b></td></tr>"
                       "<tr><td>类型</td><td><b>%6</b></td></tr>"
                       "<tr><td>别名</td><td>%7</td></tr>"
                       "<tr><td>优先级</td><td>%8（%9 分）</td></tr>"
                       "<tr><td>状态</td><td>%10</td></tr>"
                       "<tr><td>创建时间</td><td>%11</td></tr>"
                       "<tr><td>更新时间</td><td>%12</td></tr>"
                       "</table>"
                       "<h4 style='color:%1; margin:12px 0 4px 0;'>证据记忆</h4><p>%13</p>")
            .arg(theme::textPrimary(), QString::fromStdString(entity->name),
                 theme::textSecondary())
            .arg(QString::fromStdString(entity->description).toHtmlEscaped(),
                 QString::fromStdString(entity->id), theme::entityTypeLabel(entity->type),
                 aliases.isEmpty() ? QStringLiteral("—") : aliases.join(QStringLiteral("、")))
            .arg(fmt::importanceDots(entity->priority))
            .arg(entity->priority)
            .arg(status)
            .arg(fmt::timeText(entity->createdAt), fmt::timeText(entity->updatedAt),
                 memories.isEmpty() ? QStringLiteral("—") : memories.join(QStringLiteral("、"))));
}

void GraphPage::createEntity() {
    EntityEditDialog dialog(context_, this);
    if (dialog.exec() != QDialog::Accepted) return;
    try {
        const std::string id = context_->addEntity(dialog.toNewEntity());
        emit toastRequested(
            QStringLiteral("已新增实体，ID = %1").arg(QString::fromStdString(id)), false);
    } catch (const std::exception& ex) {
        emit toastRequested(QStringLiteral("新增失败：%1").arg(QString::fromUtf8(ex.what())), true);
    }
}

void GraphPage::createRelation() {
    RelationEditDialog dialog(context_, this);
    if (dialog.exec() != QDialog::Accepted) return;
    const auto result = dialog.result();

    memory::RememberRequest request;
    try {
        request.memory = memory::Memory::create(
            QStringLiteral("rel-%1")
                .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")))
                .toStdString(),
            QStringLiteral("%1 %2 %3").arg(result.subject, result.relation, result.object)
                .toStdString(),
            3, memory::MemoryType::Other);
    } catch (const std::exception& ex) {
        emit toastRequested(
            QStringLiteral("创建证据记忆失败：%1").arg(QString::fromUtf8(ex.what())), true);
        return;
    }

    memory::RelationDraft draft;
    draft.subject = result.subject.toStdString();
    draft.object = result.object.toStdString();
    draft.relation = result.relation.toStdString();
    draft.description = result.description.toStdString();
    draft.source = result.source;
    draft.priority = result.priority;
    draft.confidence = result.confidence;
    draft.eventTime = result.eventTime;
    request.relations.push_back(draft);

    try {
        const auto outcome = context_->remember(request);
        emit toastRequested(
            QStringLiteral("已建立关系 %1 → %2，生成 %3 条边%4")
                .arg(result.subject, result.object)
                .arg(outcome.edgeIds.size())
                .arg(outcome.placeholderEntityIds.empty()
                         ? QString()
                         : QStringLiteral("（其中 %1 个端点为新建占位实体）")
                               .arg(outcome.placeholderEntityIds.size())),
            false);
    } catch (const std::exception& ex) {
        emit toastRequested(
            QStringLiteral("写入失败，事务已回滚：%1").arg(QString::fromUtf8(ex.what())), true);
    }
}

void GraphPage::openAdvancedEntry() {
    RememberDialog dialog(context_, this);
    connect(&dialog, &RememberDialog::completed, this,
            [this](const QString& summary) { emit toastRequested(summary, false); });
    dialog.exec();
}

void GraphPage::editSelectedEntity() {
    const QString id = selectedEntityId();
    memory::Entity* entity =
        id.isEmpty() ? nullptr : context_->graph().findEntity(id.toStdString());
    if (entity == nullptr) {
        emit toastRequested(QStringLiteral("请先选择一个实体"), true);
        return;
    }
    EntityEditDialog dialog(context_, this);
    dialog.loadForEdit(*entity);
    if (dialog.exec() != QDialog::Accepted) return;
    try {
        memory::GraphTransaction transaction(context_->graph());
        const bool ok = context_->graph().updateEntity(
            dialog.editId().toStdString(), dialog.entityName().toStdString(),
            dialog.description().toStdString(), dialog.aliases(), dialog.priority(),
            dialog.entityType());
        if (!ok) {
            emit toastRequested(QStringLiteral("实体不存在"), true);
            return;
        }
        transaction.commit();
    } catch (const std::exception& ex) {
        emit toastRequested(QStringLiteral("更新失败：%1").arg(QString::fromUtf8(ex.what())), true);
        return;
    }
    context_->log(QStringLiteral("entity"), id, true, QStringLiteral("实体属性已更新"));
    emit toastRequested(QStringLiteral("实体 %1 已更新").arg(id), false);
    refresh();
}

void GraphPage::invalidateSelectedEntity() {
    const QString id = selectedEntityId();
    const memory::Entity* entity =
        id.isEmpty() ? nullptr : context_->graph().findEntity(id.toStdString());
    if (entity == nullptr || !entity->active) {
        emit toastRequested(QStringLiteral("请先选择一个活跃实体"), true);
        return;
    }
    const auto answer = QMessageBox::question(
        this, QStringLiteral("确认逻辑删除"),
        QStringLiteral("将把实体「%1」标记为失效，并连带失效它的所有关联边。\n"
                       "历史与证据不会被物理删除。是否继续？")
            .arg(QString::fromStdString(entity->name)));
    if (answer != QMessageBox::Yes) return;
    context_->forgetEntity(id.toStdString(), QStringLiteral("用户在界面上标记失效"));
    emit toastRequested(QStringLiteral("实体已逻辑删除"), false);
    refresh();
}

void GraphPage::mergePlaceholder() {
    const QString placeholderId = selectedEntityId();
    if (placeholderId.isEmpty()) {
        emit toastRequested(QStringLiteral("请先在列表中选择占位实体"), true);
        return;
    }
    QStringList candidates;
    QHash<QString, QString> nameToId;
    for (const auto& entity : context_->graph().entities()) {
        if (!entity.active) continue;
        if (QString::fromStdString(entity.id) == placeholderId) continue;
        const QString name = QString::fromStdString(entity.name);
        candidates << name;
        nameToId.insert(name, QString::fromStdString(entity.id));
    }
    if (candidates.isEmpty()) {
        emit toastRequested(QStringLiteral("没有可合并的真实实体"), true);
        return;
    }
    bool accepted = false;
    const QString chosen = QInputDialog::getItem(
        this, QStringLiteral("合并占位实体"),
        QStringLiteral("把「%1」并到哪个真实实体？").arg(placeholderId), candidates, 0, false,
        &accepted);
    if (!accepted || chosen.isEmpty()) return;
    if (context_->resolvePlaceholder(placeholderId.toStdString(),
                                     nameToId.value(chosen).toStdString())) {
        emit toastRequested(QStringLiteral("占位实体已合并到「%1」").arg(chosen), false);
        refresh();
    } else {
        emit toastRequested(QStringLiteral("合并失败：目标或占位实体不可用"), true);
    }
}

void GraphPage::reviseSelectedEdge() {
    const QString edgeId = selectedEdgeId();
    const memory::Edge* edge =
        edgeId.isEmpty() ? nullptr : context_->graph().findEdge(edgeId.toStdString());
    if (edge == nullptr) {
        emit toastRequested(QStringLiteral("请先选择一条关系"), true);
        return;
    }
    const std::vector<std::string> evidence = edge->memoryIds;
    RelationEditDialog dialog(context_, this);
    if (dialog.exec() != QDialog::Accepted) return;
    const auto result = dialog.result();
    const std::string newEdgeId = context_->reviseEdge(
        edgeId.toStdString(), result.relation.toStdString(), result.description.toStdString(),
        evidence, result.eventTime, result.confidence, result.priority);
    if (newEdgeId.empty()) {
        emit toastRequested(QStringLiteral("修改失败：关系不存在"), true);
        return;
    }
    emit toastRequested(
        QStringLiteral("旧关系已失效，新关系 %1 已生效（历史保留）")
            .arg(QString::fromStdString(newEdgeId)),
        false);
    refresh();
}

void GraphPage::invalidateSelectedEdge() {
    const QString edgeId = selectedEdgeId();
    if (edgeId.isEmpty()) {
        emit toastRequested(QStringLiteral("请先选择一条关系"), true);
        return;
    }
    const auto answer = QMessageBox::question(
        this, QStringLiteral("确认逻辑删除"),
        QStringLiteral("将把关系 %1 标记为失效（保留历史）。是否继续？").arg(edgeId));
    if (answer != QMessageBox::Yes) return;
    if (context_->forgetEdge(edgeId.toStdString(), QStringLiteral("用户在界面上标记失效"))) {
        emit toastRequested(QStringLiteral("关系已逻辑删除"), false);
        refresh();
    } else {
        emit toastRequested(QStringLiteral("关系不存在或已失效"), true);
    }
}

void GraphPage::runBfs() {
    QString startId = canvas_->selectedNodeId();
    if (startId.isEmpty()) startId = selectedEntityId();
    if (startId.isEmpty()) {
        emit toastRequested(QStringLiteral("请先选择一个起点实体"), true);
        return;
    }
    memory::BfsOptions options;
    options.maxDepth = depthSpin_->value();
    options.limit = static_cast<std::size_t>(limitSpin_->value());
    options.includeInactive = showInactiveEdges_->isChecked();
    options.undirected = undirectedCheck_->isChecked();
    options.keyword = keywordEdit_->text().trimmed().toStdString();

    const auto items = context_->graph().bfs(startId.toStdString(), options);
    traversalList_->clear();
    for (const auto& item : items) {
        const memory::Entity* entity = context_->graph().findEntity(item.entityId);
        traversalList_->addItem(
            QStringLiteral("深度 %1  --%2-->  %3   (score %4)")
                .arg(item.depth)
                .arg(QString::fromStdString(item.relation),
                     entity == nullptr ? QString::fromStdString(item.entityId)
                                       : QString::fromStdString(entity->name),
                     fmt::number(item.score, 3)));
    }
    if (items.empty()) traversalList_->addItem(QStringLiteral("没有可达的邻居。"));
    context_->log(QStringLiteral("bfs"), startId, true,
                  QStringLiteral("返回 %1 条邻域").arg(items.size()));
}

void GraphPage::runDfs() {
    QString startId = canvas_->selectedNodeId();
    if (startId.isEmpty()) startId = selectedEntityId();
    if (startId.isEmpty()) {
        emit toastRequested(QStringLiteral("请先选择一个起点实体"), true);
        return;
    }
    memory::DfsOptions options;
    options.maxDepth = depthSpin_->value();
    options.limit = static_cast<std::size_t>(limitSpin_->value());
    options.includeInactive = showInactiveEdges_->isChecked();
    options.undirected = undirectedCheck_->isChecked();
    options.keyword = keywordEdit_->text().trimmed().toStdString();

    const auto paths = context_->graph().dfs(startId.toStdString(), options);
    traversalList_->clear();
    for (const auto& path : paths) {
        QStringList steps;
        for (const auto& step : path.steps) {
            const memory::Entity* entity = context_->graph().findEntity(step.entityId);
            const QString name = entity == nullptr ? QString::fromStdString(step.entityId)
                                                   : QString::fromStdString(entity->name);
            if (step.relation.empty()) {
                steps << name;
            } else {
                steps << QStringLiteral("--%1--> %2")
                             .arg(QString::fromStdString(step.relation), name);
            }
        }
        traversalList_->addItem(QStringLiteral("%1   (score %2)")
                                    .arg(steps.join(QLatin1Char(' ')))
                                    .arg(fmt::number(path.score, 3)));
    }
    if (paths.empty()) traversalList_->addItem(QStringLiteral("没有可展开的推理链。"));
    context_->log(QStringLiteral("dfs"), startId, true,
                  QStringLiteral("返回 %1 条路径").arg(paths.size()));
}

void GraphPage::exportImage() {
    const QString fileName = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出画布"), QStringLiteral("graph.png"),
        QStringLiteral("PNG 图片 (*.png)"));
    if (fileName.isEmpty()) return;
    if (canvas_->renderToPixmap().save(fileName)) {
        emit toastRequested(QStringLiteral("已导出到 %1").arg(fileName), false);
    } else {
        emit toastRequested(QStringLiteral("导出失败"), true);
    }
}

}  // namespace gui
