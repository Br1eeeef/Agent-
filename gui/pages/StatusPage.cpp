#include "pages/StatusPage.h"

#include "Format.h"
#include "models/LogTableModel.h"
#include "theme/Theme.h"
#include "widgets/Card.h"
#include "widgets/KpiCard.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QTableView>
#include <QVBoxLayout>

namespace gui {

StatusPage::StatusPage(AppContext* context, QWidget* parent)
    : QWidget(parent), context_(context) {
    logModel_ = new LogTableModel(context, this);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(14);
    root->addWidget(buildCards());

    auto* middleRow = new QHBoxLayout();
    middleRow->setSpacing(14);
    middleRow->addWidget(buildQueueCard(), 3);
    middleRow->addWidget(buildLruCard(), 2);
    root->addLayout(middleRow, 1);

    root->addWidget(buildLogCard(), 2);
    root->addWidget(buildDangerCard());

    connect(context_, &AppContext::memoriesChanged, this, &StatusPage::refresh);
    connect(context_, &AppContext::graphChanged, this, &StatusPage::refresh);
    connect(context_, &AppContext::logAppended, this, &StatusPage::refresh);
    refresh();
}

QWidget* StatusPage::buildCards() {
    auto* host = new QWidget(this);
    auto* layout = new QHBoxLayout(host);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);

    queueCard_ = new KpiCard(QStringLiteral("短期队列"), QStringLiteral("◔"), host);
    queueCard_->setAccent(QColor(theme::success()));
    lruCard_ = new KpiCard(QStringLiteral("LRU 活跃索引"), QStringLiteral("⇄"), host);
    lruCard_->setAccent(QColor("#F2994A"));
    hashCard_ = new KpiCard(QStringLiteral("哈希表"), QStringLiteral("#"), host);
    hashCard_->setAccent(QColor(theme::accent()));
    fileCard_ = new KpiCard(QStringLiteral("数据文件"), QStringLiteral("▤"), host);
    fileCard_->setAccent(QColor("#7C5CFC"));

    for (auto* card : {queueCard_, lruCard_, hashCard_, fileCard_}) layout->addWidget(card, 1);
    return host;
}

QWidget* StatusPage::buildQueueCard() {
    auto* body = new QWidget(this);
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    queueHeadLabel_ = new QLabel(body);
    queueHeadLabel_->setWordWrap(true);
    queueHeadLabel_->setStyleSheet(
        QStringLiteral("QLabel { color: %1; }").arg(theme::textSecondary()));
    layout->addWidget(queueHeadLabel_);

    queueGridHost_ = new QWidget(body);
    queueGrid_ = new QGridLayout(queueGridHost_);
    queueGrid_->setContentsMargins(0, 0, 0, 0);
    queueGrid_->setSpacing(4);
    layout->addWidget(queueGridHost_);
    layout->addStretch(1);
    return makeCard(QStringLiteral("短期循环队列（20 格，左侧为队列头）"), body, this);
}

QWidget* StatusPage::buildLruCard() {
    auto* body = new QWidget(this);
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    lruHintLabel_ = new QLabel(body);
    lruHintLabel_->setWordWrap(true);
    lruHintLabel_->setStyleSheet(QStringLiteral("QLabel { color: %1; }").arg(theme::textMuted()));
    layout->addWidget(lruHintLabel_);

    auto* scroll = new QScrollArea(body);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    lruListHost_ = new QWidget(scroll);
    auto* hostLayout = new QVBoxLayout(lruListHost_);
    hostLayout->setContentsMargins(0, 0, 0, 0);
    hostLayout->setSpacing(4);
    hostLayout->setAlignment(Qt::AlignTop);
    scroll->setWidget(lruListHost_);
    layout->addWidget(scroll, 1);
    return makeCard(QStringLiteral("LRU 顺序（最近访问在前）"), body, this);
}

QWidget* StatusPage::buildLogCard() {
    auto* body = new QWidget(this);
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* toolbar = new QHBoxLayout();
    logFilter_ = new QComboBox(body);
    logFilter_->addItem(QStringLiteral("全部操作"), QString());
    for (const char* action : {"add", "update", "remove", "recall", "find", "save", "load",
                               "remember", "forget", "revise", "entity", "seed", "clear"}) {
        logFilter_->addItem(QString::fromUtf8(action), QString::fromUtf8(action));
    }
    toolbar->addWidget(logFilter_);
    onlyFailures_ = new QCheckBox(QStringLiteral("只看失败"), body);
    toolbar->addWidget(onlyFailures_);
    toolbar->addStretch(1);
    auto* copyButton = new QPushButton(QStringLiteral("复制选中行"), body);
    auto* clearButton = new QPushButton(QStringLiteral("清空日志"), body);
    toolbar->addWidget(copyButton);
    toolbar->addWidget(clearButton);
    layout->addLayout(toolbar);

    logTable_ = new QTableView(body);
    logTable_->setModel(logModel_);
    logTable_->setFrameShape(QFrame::NoFrame);
    logTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    logTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    logTable_->verticalHeader()->setVisible(false);
    logTable_->verticalHeader()->setDefaultSectionSize(28);
    logTable_->setShowGrid(false);
    logTable_->horizontalHeader()->setSectionResizeMode(LogTableModel::DetailColumn,
                                                        QHeaderView::Stretch);
    layout->addWidget(logTable_, 1);

    connect(logFilter_, &QComboBox::currentIndexChanged, this, [this](int) {
        logModel_->setActionFilter(logFilter_->currentData().toString());
    });
    connect(onlyFailures_, &QCheckBox::toggled, this,
            [this](bool checked) { logModel_->setOnlyFailures(checked); });
    connect(copyButton, &QPushButton::clicked, this, &StatusPage::copySelectedLog);
    connect(clearButton, &QPushButton::clicked, this, [this]() {
        context_->logger().clear();
        context_->log(QStringLiteral("clear"), QStringLiteral("log"), true,
                      QStringLiteral("操作日志已清空"));
    });
    return makeCard(QStringLiteral("操作日志（失败操作以红色行留痕）"), body, this);
}

QWidget* StatusPage::buildDangerCard() {
    auto* body = new QWidget(this);
    auto* layout = new QHBoxLayout(body);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    auto* hint = new QLabel(
        QStringLiteral("以下操作只影响内存中的数据，磁盘上的 JSON 文件不会被删除。"), body);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("QLabel { color: %1; }").arg(theme::textSecondary()));
    layout->addWidget(hint, 1);

    auto* clearButton = new QPushButton(QStringLiteral("清空全部数据"), body);
    clearButton->setProperty("variant", "danger");
    auto* reseedButton = new QPushButton(QStringLiteral("用示例数据重新播种"), body);
    layout->addWidget(reseedButton);
    layout->addWidget(clearButton);

    connect(clearButton, &QPushButton::clicked, this, [this]() {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("确认清空"),
            QStringLiteral("将清空内存中的全部记忆、实体与关系，磁盘文件保持不变。是否继续？"));
        if (answer != QMessageBox::Yes) return;
        context_->clearAll();
        emit toastRequested(QStringLiteral("已清空内存中的全部数据"), false);
    });
    connect(reseedButton, &QPushButton::clicked, this, [this]() {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("确认播种"),
            QStringLiteral("将丢弃当前内存数据，并重新载入示例记忆与示例图谱。是否继续？"));
        if (answer != QMessageBox::Yes) return;
        context_->seedSampleData();
        emit toastRequested(QStringLiteral("已重新播种示例数据"), false);
    });
    return makeCard(QStringLiteral("危险操作"), body, this);
}

void StatusPage::refresh() {
    if (context_ == nullptr) return;
    const auto& manager = context_->manager();
    const auto& graph = context_->graph();

    queueCard_->setValue(QStringLiteral("%1 / %2")
                             .arg(manager.shortTermSize())
                             .arg(manager.shortTermCapacity()));
    queueCard_->setSubtitle(QStringLiteral("超出容量时覆盖最旧元素"));

    const auto lruOrder = manager.lruOrder();
    lruCard_->setValue(QStringLiteral("%1 / %2")
                           .arg(manager.lruSize())
                           .arg(manager.lruCapacity()));
    lruCard_->setSubtitle(QStringLiteral("本次会话 find %1 次 · recall %2 次")
                              .arg(context_->logger().countOfAction(QStringLiteral("find")))
                              .arg(context_->logger().countOfAction(QStringLiteral("recall"))));

    hashCard_->setValue(QString::number(manager.size()));
    hashCard_->setSubtitle(QStringLiteral("桶 %1 个 · 负载因子 %2 · 平均链长 %3")
                               .arg(manager.bucketCount())
                               .arg(fmt::number(manager.hashLoadFactor(), 3))
                               .arg(manager.bucketCount() == 0
                                        ? QStringLiteral("—")
                                        : fmt::number(
                                              static_cast<double>(manager.size()) /
                                                  static_cast<double>(manager.bucketCount()),
                                              2)));

    const QFileInfo memoryFile(context_->memoriesPath());
    const QFileInfo graphFile(context_->graphPath());
    fileCard_->setValue(QStringLiteral("M %1 KB · G %2 KB")
                            .arg(memoryFile.exists() ? memoryFile.size() / 1024 : 0)
                            .arg(graphFile.exists() ? graphFile.size() / 1024 : 0));
    fileCard_->setSubtitle(
        QStringLiteral("%1；最后保存 %2")
            .arg(context_->isDirty() ? QStringLiteral("有未保存改动") : QStringLiteral("与磁盘一致"),
                 fmt::dateTimeText(context_->lastSavedAt())));

    const auto recentIds = manager.recentIds();
    queueHeadLabel_->setText(
        QStringLiteral("队列头 %1　队列尾 %2　当前 %3/%4　实体 %5 / 关系 %6")
            .arg(recentIds.empty() ? QStringLiteral("—")
                                   : QString::fromStdString(recentIds.front()),
                 recentIds.empty() ? QStringLiteral("—")
                                   : QString::fromStdString(recentIds.back()))
            .arg(recentIds.size())
            .arg(manager.shortTermCapacity())
            .arg(graph.activeEntityCount())
            .arg(graph.activeEdgeCount()));
    rebuildQueueGrid();
    rebuildLruList();
    logModel_->refresh();
}

void StatusPage::rebuildQueueGrid() {
    const auto recentIds = context_->manager().recentIds();
    const int capacity = static_cast<int>(context_->manager().shortTermCapacity());
    while (queueGrid_->count() > 0) {
        QLayoutItem* item = queueGrid_->takeAt(0);
        if (item->widget() != nullptr) {
            // 先摘除父子关系再延迟析构：deleteLater 在当前事件循环结束前不会真正销毁，
            // 若直接留着会与新控件重叠显示。
            item->widget()->hide();
            item->widget()->setParent(nullptr);
            item->widget()->deleteLater();
        }
        delete item;
    }
    queueCells_.clear();

    // 每行 10 格，便于在 20 容量下正好两行。
    const int perRow = 10;
    for (int i = 0; i < capacity; ++i) {
        auto* cell = new QLabel(queueGridHost_);
        cell->setAlignment(Qt::AlignCenter);
        cell->setFixedHeight(30);
        const bool filled = i < static_cast<int>(recentIds.size());
        const bool isHead = filled && i == 0;
        const bool isTail = filled && i == static_cast<int>(recentIds.size()) - 1;
        QString text = QString::number(i + 1);
        if (filled) {
            const QString id = QString::fromStdString(recentIds[static_cast<std::size_t>(i)]);
            text = id.size() > 11 ? id.left(9) + QStringLiteral("…") : id;
        }
        if (filled && context_->manager().peek(recentIds[static_cast<std::size_t>(i)]) == nullptr) {
            text += QStringLiteral(" ✕");
        }
        cell->setText(text);

        QString style;
        if (isHead) {
            style = QStringLiteral("background:%1; color:white; border-radius:6px;"
                                   " font-weight:600; padding:2px;")
                        .arg(theme::success());
        } else if (isTail) {
            style = QStringLiteral("background:%1; color:%2; border-radius:6px;"
                                   " font-weight:600; padding:2px;")
                        .arg(theme::alpha(theme::success(), 0.22), theme::success());
        } else if (filled) {
            style = QStringLiteral("background:%1; color:%2; border-radius:6px; padding:2px;")
                        .arg(theme::rowSelected(), theme::textPrimary());
        } else {
            style = QStringLiteral("background:%1; color:%2; border-radius:6px; padding:2px;"
                                   " border:1px dashed %3;")
                        .arg(theme::rowZebra(), theme::textMuted(), theme::border());
        }
        cell->setStyleSheet(QStringLiteral("QLabel { %1 }").arg(style));
        cell->setToolTip(filled ? QStringLiteral("位置 %1：%2")
                                      .arg(i + 1)
                                      .arg(QString::fromStdString(
                                          recentIds[static_cast<std::size_t>(i)]))
                                : QStringLiteral("空位 %1").arg(i + 1));
        queueGrid_->addWidget(cell, i / perRow, i % perRow);
        queueCells_.push_back(cell);
    }
}

void StatusPage::rebuildLruList() {
    auto* layout = qobject_cast<QVBoxLayout*>(lruListHost_->layout());
    if (layout == nullptr) return;
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (item->widget() != nullptr) {
            item->widget()->hide();
            item->widget()->setParent(nullptr);
            item->widget()->deleteLater();
        }
        delete item;
    }

    const auto order = context_->manager().lruOrder();
    const int capacity = static_cast<int>(context_->manager().lruCapacity());
    lruHintLabel_->setText(
        capacity == 0
            ? QStringLiteral("LRU 容量为 0。")
            : QStringLiteral("容量 %1，当前 %2 条。每次读取记忆都会把该 ID 移到最前，"
                             "写满后淘汰最久未访问项。")
                  .arg(capacity)
                  .arg(order.size()));

    int index = 0;
    for (const auto& id : order) {
        ++index;
        auto* row = new QWidget(lruListHost_);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(6, 2, 6, 2);
        rowLayout->setSpacing(8);

        auto* label = new QLabel(
            QStringLiteral("%1. %2").arg(index).arg(QString::fromStdString(id)), row);
        label->setStyleSheet(
            QStringLiteral("QLabel { color: %1; }")
                .arg(index <= 3 ? theme::textPrimary() : theme::textSecondary()));
        rowLayout->addWidget(label, 1);

        auto* hint = new QLabel(
            QStringLiteral("距淘汰 %1").arg(qMax(0, capacity - index)), row);
        hint->setStyleSheet(
            QStringLiteral("QLabel { color: %1; }").arg(theme::textMuted()));
        rowLayout->addWidget(hint);

        auto* touchButton = new QPushButton(QStringLiteral("访问"), row);
        touchButton->setFixedWidth(56);
        const QString target = QString::fromStdString(id);
        connect(touchButton, &QPushButton::clicked, this, [this, target]() {
            if (context_->touch(target) != nullptr) {
                emit toastRequested(
                    QStringLiteral("已访问 %1，LRU 顺序已更新").arg(target), false);
            } else {
                emit toastRequested(QStringLiteral("%1 已不在长期记忆中").arg(target), true);
            }
        });
        rowLayout->addWidget(touchButton);
        layout->addWidget(row);
    }
    if (order.empty()) {
        auto* empty = new QLabel(
            QStringLiteral("暂无访问记录：新增记忆或点击检索结果卡片会写入这里。"), lruListHost_);
        empty->setStyleSheet(QStringLiteral("QLabel { color: %1; }").arg(theme::textMuted()));
        layout->addWidget(empty);
    }
}

void StatusPage::copySelectedLog() {
    const QModelIndexList rows = logTable_->selectionModel()->selectedRows();
    QStringList lines;
    const auto entries = logModel_->visibleEntries();
    for (const auto& index : rows) {
        if (index.row() < 0 || index.row() >= entries.size()) continue;
        const LogEntry& entry = entries.at(index.row());
        lines << QStringLiteral("[%1] %2 %3 %4 %5")
                     .arg(entry.time.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
                          entry.action, entry.target,
                          entry.ok ? QStringLiteral("成功") : QStringLiteral("失败"),
                          entry.detail);
    }
    if (lines.isEmpty()) {
        emit toastRequested(QStringLiteral("请先选择要复制的日志行"), true);
        return;
    }
    QApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
    emit toastRequested(QStringLiteral("已复制 %1 行日志").arg(lines.size()), false);
}

}  // namespace gui
