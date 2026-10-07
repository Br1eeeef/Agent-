#include "pages/MemoryPage.h"

#include "Format.h"
#include "dialogs/MemoryEditDialog.h"
#include "dialogs/RememberDialog.h"
#include "models/MemoryTableModel.h"
#include "theme/Theme.h"
#include "widgets/Card.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTableView>
#include <QTextBrowser>
#include <QVBoxLayout>

#include <algorithm>

namespace gui {

MemoryPage::MemoryPage(AppContext* context, QWidget* parent)
    : QWidget(parent), context_(context) {
    model_ = new MemoryTableModel(context, this);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(12);
    root->addWidget(buildToolbar());

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(buildTable());
    splitter->addWidget(buildDetail());
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    splitter->setChildrenCollapsible(false);
    splitter->setSizes({840, 560});
    root->addWidget(splitter, 1);

    connect(context_, &AppContext::memoriesChanged, this, [this]() {
        const QString keep = selectedId();
        refresh();
        if (!keep.isEmpty()) selectMemory(keep);
    });
    refresh();
}

QWidget* MemoryPage::buildToolbar() {
    auto* bar = new QWidget(this);
    auto* layout = new QVBoxLayout(bar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* filterRow = new QHBoxLayout();
    filterRow->setSpacing(8);

    searchEdit_ = new QLineEdit(bar);
    searchEdit_->setPlaceholderText(QStringLiteral("搜索 ID / 内容 / 关键词"));
    searchEdit_->setClearButtonEnabled(true);
    searchEdit_->setMinimumWidth(220);
    filterRow->addWidget(searchEdit_, 2);

    typeCombo_ = new QComboBox(bar);
    typeCombo_->addItem(QStringLiteral("全部类型"), -1);
    typeCombo_->addItem(theme::memoryTypeLabel(memory::MemoryType::Profile),
                        static_cast<int>(memory::MemoryType::Profile));
    typeCombo_->addItem(theme::memoryTypeLabel(memory::MemoryType::Plan),
                        static_cast<int>(memory::MemoryType::Plan));
    typeCombo_->addItem(theme::memoryTypeLabel(memory::MemoryType::Conversation),
                        static_cast<int>(memory::MemoryType::Conversation));
    typeCombo_->addItem(theme::memoryTypeLabel(memory::MemoryType::Event),
                        static_cast<int>(memory::MemoryType::Event));
    typeCombo_->addItem(theme::memoryTypeLabel(memory::MemoryType::Other),
                        static_cast<int>(memory::MemoryType::Other));
    filterRow->addWidget(typeCombo_);

    minImportance_ = new QSpinBox(bar);
    minImportance_->setRange(1, 5);
    minImportance_->setPrefix(QStringLiteral("重要度 ≥ "));
    filterRow->addWidget(minImportance_);

    maxImportance_ = new QSpinBox(bar);
    maxImportance_->setRange(1, 5);
    maxImportance_->setValue(5);
    maxImportance_->setPrefix(QStringLiteral("≤ "));
    filterRow->addWidget(maxImportance_);

    sortCombo_ = new QComboBox(bar);
    sortCombo_->addItem(QStringLiteral("按创建时间（新→旧）"),
                        MemoryTableModel::ByCreatedDesc);
    sortCombo_->addItem(QStringLiteral("按重要度（高→低）"),
                        MemoryTableModel::ByImportanceDesc);
    sortCombo_->addItem(QStringLiteral("按最近访问"), MemoryTableModel::ByAccessedDesc);
    sortCombo_->addItem(QStringLiteral("按 ID"), MemoryTableModel::ByIdAsc);
    filterRow->addWidget(sortCombo_);
    filterRow->addStretch(1);
    layout->addLayout(filterRow);

    auto* actionRow = new QHBoxLayout();
    actionRow->setSpacing(8);
    auto* addButton = new QPushButton(QStringLiteral("新增记忆"), bar);
    addButton->setProperty("variant", "primary");
    auto* advancedButton = new QPushButton(QStringLiteral("高级录入（记忆+实体+关系）"), bar);
    auto* saveButton = new QPushButton(QStringLiteral("保存到 data/"), bar);
    auto* refreshButton = new QPushButton(QStringLiteral("刷新"), bar);
    editButton_ = new QPushButton(QStringLiteral("编辑"), bar);
    removeButton_ = new QPushButton(QStringLiteral("删除"), bar);
    removeButton_->setProperty("variant", "danger");

    actionRow->addWidget(addButton);
    actionRow->addWidget(advancedButton);
    actionRow->addWidget(editButton_);
    actionRow->addWidget(removeButton_);
    actionRow->addStretch(1);
    summaryLabel_ = new QLabel(bar);
    summaryLabel_->setStyleSheet(
        QStringLiteral("QLabel { color: %1; }").arg(theme::textSecondary()));
    actionRow->addWidget(summaryLabel_);
    actionRow->addWidget(saveButton);
    actionRow->addWidget(refreshButton);
    layout->addLayout(actionRow);

    connect(searchEdit_, &QLineEdit::textChanged, this,
            [this](const QString& text) { model_->setTextFilter(text); });
    connect(typeCombo_, &QComboBox::currentIndexChanged, this, [this](int) {
        std::vector<memory::MemoryType> types;
        const int value = typeCombo_->currentData().toInt();
        if (value >= 0) types.push_back(static_cast<memory::MemoryType>(value));
        model_->setTypeFilter(types);
        summaryLabel_->setText(QStringLiteral("显示 %1 条").arg(model_->visibleCount()));
    });
    connect(minImportance_, &QSpinBox::valueChanged, this, [this](int) {
        model_->setImportanceRange(minImportance_->value(), maxImportance_->value());
    });
    connect(maxImportance_, &QSpinBox::valueChanged, this, [this](int) {
        model_->setImportanceRange(minImportance_->value(), maxImportance_->value());
    });
    connect(sortCombo_, &QComboBox::currentIndexChanged, this, [this](int) {
        model_->setSortMode(static_cast<MemoryTableModel::SortMode>(
            sortCombo_->currentData().toInt()));
    });
    connect(addButton, &QPushButton::clicked, this, &MemoryPage::createMemory);
    connect(advancedButton, &QPushButton::clicked, this, &MemoryPage::openAdvancedEntry);
    connect(editButton_, &QPushButton::clicked, this, &MemoryPage::editSelected);
    connect(removeButton_, &QPushButton::clicked, this, &MemoryPage::removeSelected);
    connect(refreshButton, &QPushButton::clicked, this, &MemoryPage::refresh);
    connect(saveButton, &QPushButton::clicked, this, [this]() {
        QString error;
        if (context_->saveAll(&error)) {
            emit toastRequested(QStringLiteral("记忆与图谱已保存到 data/"), false);
        } else {
            emit toastRequested(QStringLiteral("保存失败：%1").arg(error), true);
        }
    });
    return bar;
}

QWidget* MemoryPage::buildTable() {
    table_ = new QTableView(this);
    table_->setModel(model_);
    table_->setFrameShape(QFrame::NoFrame);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    table_->verticalHeader()->setVisible(false);
    table_->verticalHeader()->setDefaultSectionSize(34);
    table_->horizontalHeader()->setHighlightSections(false);
    table_->horizontalHeader()->setMinimumSectionSize(60);
    // ID 固定宽度，正文占满剩余空间，其余列按内容自适应。
    table_->horizontalHeader()->setSectionResizeMode(MemoryTableModel::IdColumn,
                                                     QHeaderView::Interactive);
    table_->horizontalHeader()->resizeSection(MemoryTableModel::IdColumn, 112);
    table_->horizontalHeader()->setSectionResizeMode(MemoryTableModel::ContentColumn,
                                                     QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(MemoryTableModel::TypeColumn,
                                                     QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(MemoryTableModel::ImportanceColumn,
                                                     QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(MemoryTableModel::KeywordsColumn,
                                                     QHeaderView::Interactive);
    table_->horizontalHeader()->resizeSection(MemoryTableModel::KeywordsColumn, 98);
    table_->horizontalHeader()->setSectionResizeMode(MemoryTableModel::CreatedColumn,
                                                     QHeaderView::Interactive);
    table_->horizontalHeader()->resizeSection(MemoryTableModel::CreatedColumn, 84);
    table_->horizontalHeader()->setSectionResizeMode(MemoryTableModel::AccessedColumn,
                                                     QHeaderView::Interactive);
    table_->horizontalHeader()->resizeSection(MemoryTableModel::AccessedColumn, 84);
    table_->setShowGrid(false);

    connect(table_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this]() { updateDetail(); });
    connect(table_, &QTableView::doubleClicked, this, [this]() { editSelected(); });
    return makeCard(QStringLiteral("记忆列表"), table_, this);
}

QWidget* MemoryPage::buildDetail() {
    auto* body = new QWidget(this);
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    detailBrowser_ = new QTextBrowser(body);
    detailBrowser_->setFrameShape(QFrame::NoFrame);
    detailBrowser_->setOpenExternalLinks(false);
    layout->addWidget(detailBrowser_, 1);

    recallButton_ = new QPushButton(QStringLiteral("在检索页以此记忆为查询"), body);
    layout->addWidget(recallButton_);
    connect(recallButton_, &QPushButton::clicked, this, [this]() {
        const QString id = selectedId();
        if (id.isEmpty()) return;
        const memory::Memory* value = context_->manager().peek(id.toStdString());
        if (value != nullptr) emit recallRequested(QString::fromStdString(value->content));
    });

    return makeCard(QStringLiteral("记忆详情"), body, this);
}

QString MemoryPage::selectedId() const {
    const QModelIndexList rows = table_->selectionModel()->selectedRows();
    if (rows.isEmpty()) return {};
    return model_->idAt(rows.first().row());
}

void MemoryPage::refresh() {
    model_->refresh();
    summaryLabel_->setText(QStringLiteral("显示 %1 条%2")
                               .arg(model_->visibleCount())
                               .arg(model_->ghostCount() > 0
                                        ? QStringLiteral("（其中 %1 条为短期队列残留）")
                                              .arg(model_->ghostCount())
                                        : QString()));
    updateDetail();
}

void MemoryPage::selectMemory(const QString& id) {
    const int row = model_->rowOfId(id);
    if (row < 0) return;
    table_->selectRow(row);
    table_->scrollTo(model_->index(row, 0));
    updateDetail();
}

void MemoryPage::createMemory() {
    MemoryEditDialog dialog(context_, this);
    if (dialog.exec() != QDialog::Accepted) return;
    QString evicted;
    QString error;
    if (!context_->addMemory(dialog.toMemory(), &evicted, &error)) {
        emit toastRequested(QStringLiteral("写入失败：%1").arg(error), true);
        return;
    }
    if (!evicted.isEmpty()) {
        emit toastRequested(
            QStringLiteral("短期队列已淘汰 %1（长期记忆保留，仍可在列表中看到）").arg(evicted),
            false);
    } else {
        emit toastRequested(QStringLiteral("已写入记忆 %1").arg(dialog.id()), false);
    }
    selectMemory(dialog.id());
}

void MemoryPage::openAdvancedEntry() {
    RememberDialog dialog(context_, this);
    connect(&dialog, &RememberDialog::completed, this, [this](const QString& summary) {
        emit toastRequested(summary, false);
    });
    dialog.exec();
}

void MemoryPage::editSelected() {
    const QString id = selectedId();
    if (id.isEmpty()) {
        emit toastRequested(QStringLiteral("请先选择一条记忆"), true);
        return;
    }
    const memory::Memory* value = context_->manager().peek(id.toStdString());
    if (value == nullptr) {
        emit toastRequested(QStringLiteral("%1 只存在于短期队列，长期记忆里已经没有这条数据。")
                                .arg(id),
                            true);
        return;
    }
    MemoryEditDialog dialog(context_, this);
    dialog.loadForEdit(*value);
    if (dialog.exec() != QDialog::Accepted) return;
    QString error;
    if (!context_->updateMemory(id, QString::fromStdString(
                                        dialog.toMemory().content),
                                dialog.toMemory().importance, dialog.keywords(), &error)) {
        emit toastRequested(QStringLiteral("更新失败：%1").arg(error), true);
        return;
    }
    emit toastRequested(QStringLiteral("已更新记忆 %1").arg(id), false);
    selectMemory(id);
}

void MemoryPage::removeSelected() {
    const QString id = selectedId();
    if (id.isEmpty()) {
        emit toastRequested(QStringLiteral("请先选择一条记忆"), true);
        return;
    }
    const auto answer = QMessageBox::question(
        this, QStringLiteral("确认删除"),
        QStringLiteral("确定要删除记忆 %1 吗？\n\n"
                       "删除只会从长期记忆与 LRU 索引中移除，短期循环队列里的残留 ID 不会被清理，"
                       "已作为证据被图引用的记录也会保留。")
            .arg(id));
    if (answer != QMessageBox::Yes) return;
    QString error;
    if (!context_->removeMemory(id, &error)) {
        emit toastRequested(QStringLiteral("删除失败：%1").arg(error), true);
        return;
    }
    emit toastRequested(QStringLiteral("已删除记忆 %1").arg(id), false);
}

void MemoryPage::updateDetail() {
    const QString id = selectedId();
    if (id.isEmpty()) {
        detailBrowser_->setHtml(
            QStringLiteral("<div style='color:%1; font-size:10pt;'>在左侧选择一条记忆查看详情。</div>")
                .arg(theme::textMuted()));
        recallButton_->setEnabled(false);
        editButton_->setEnabled(false);
        removeButton_->setEnabled(false);
        return;
    }
    recallButton_->setEnabled(true);
    editButton_->setEnabled(true);
    removeButton_->setEnabled(true);

    const memory::Memory* value = context_->manager().peek(id.toStdString());
    if (value == nullptr) {
        detailBrowser_->setHtml(
            QStringLiteral("<h3 style='color:%1'>%2</h3>"
                           "<p style='color:%1'>该 ID 只存在于短期循环队列中，长期记忆里已经没有这条数据。"
                           "这是 MemoryManager::remove 不清理短期队列的真实行为，界面如实呈现。</p>")
                .arg(theme::textMuted(), id));
        editButton_->setEnabled(false);
        recallButton_->setEnabled(false);
        return;
    }

    const auto recentIds = context_->manager().recentIds();
    const auto lruOrder = context_->manager().lruOrder();
    int lruPosition = -1;
    for (std::size_t i = 0; i < lruOrder.size(); ++i) {
        if (lruOrder[i] == value->id) {
            lruPosition = static_cast<int>(i) + 1;
            break;
        }
    }
    const bool inShortTerm = std::find(recentIds.begin(), recentIds.end(), value->id) !=
                             recentIds.end();

    QStringList keywords;
    for (const auto& keyword : value->keywords) keywords << QString::fromStdString(keyword);

    QString referenceHtml = QStringLiteral("<p style='color:%1'>暂无图引用。</p>")
                                .arg(theme::textMuted());
    QStringList references;
    for (const auto& entity : context_->graph().entities()) {
        if (std::find(entity.memoryIds.begin(), entity.memoryIds.end(), value->id) !=
                entity.memoryIds.end() ||
            entity.sourceMemoryId == value->id) {
            references << QStringLiteral("实体「%1」").arg(QString::fromStdString(entity.name));
        }
    }
    for (const auto& edge : context_->graph().edges()) {
        if (edge.hasMemoryId(value->id)) {
            references << QStringLiteral("关系 %1").arg(QString::fromStdString(edge.relation));
        }
    }
    if (!references.isEmpty()) {
        referenceHtml = QStringLiteral("<p>%1</p>").arg(references.join(QStringLiteral("、")));
    }

    detailBrowser_->setHtml(QStringLiteral(
        "<h3 style='color:%1; margin:0 0 8px 0;'>%2</h3>"
        "<p style='color:%3; margin:0 0 10px 0;'>%4</p>"
        "<table cellspacing='0' cellpadding='3' style='color:%5;'>"
        "<tr><td>类型</td><td><b>%6</b></td></tr>"
        "<tr><td>重要度</td><td><b>%7</b>（%8 分）</td></tr>"
        "<tr><td>关键词</td><td>%9</td></tr>"
        "<tr><td>创建时间</td><td>%10</td></tr>"
        "<tr><td>更新时间</td><td>%11</td></tr>"
        "<tr><td>最近访问</td><td>%12</td></tr>"
        "<tr><td>短期队列</td><td>%13</td></tr>"
        "<tr><td>LRU 位置</td><td>%14</td></tr>"
        "</table>"
        "<h4 style='color:%1; margin:12px 0 4px 0;'>图引用</h4>%15")
        .arg(theme::textPrimary(), id, theme::textSecondary(),
             QString::fromStdString(value->content).toHtmlEscaped(), theme::textSecondary(),
             theme::memoryTypeLabel(value->type), fmt::importanceDots(value->importance))
        .arg(value->importance)
        .arg(keywords.isEmpty() ? QStringLiteral("—") : keywords.join(QStringLiteral("、")),
             fmt::timeText(value->createdAt), fmt::timeText(value->updatedAt),
             fmt::timeText(value->lastAccessedAt),
             inShortTerm ? QStringLiteral("在队列中") : QStringLiteral("不在队列中"),
             lruPosition > 0 ? QStringLiteral("第 %1 位").arg(lruPosition)
                             : QStringLiteral("未在 LRU 中"))
        .arg(referenceHtml));
}

}  // namespace gui
