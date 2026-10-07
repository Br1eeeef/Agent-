#include "pages/OverviewPage.h"

#include "Format.h"
#include "theme/Theme.h"
#include "widgets/Card.h"
#include "widgets/EmptyState.h"
#include "widgets/KpiCard.h"
#include "widgets/TypePieChart.h"

#include <QBarCategoryAxis>
#include <QBarSeries>
#include <QBarSet>
#include <QChart>
#include <QChartView>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QStackedWidget>
#include <QValueAxis>
#include <QVBoxLayout>

#include <algorithm>

namespace gui {
namespace {

// 短期队列占用条：20 个格子表示容量，写满后条头转为橙色警告。
class SegmentBarWidget : public QWidget {
public:
    explicit SegmentBarWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumHeight(30);
    }

    void setState(int used, int capacity) {
        used_ = qMax(0, used);
        capacity_ = qMax(1, capacity);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const int segments = capacity_;
        const double gap = 3.0;
        const double width = (this->width() - gap * (segments - 1)) / segments;
        const double barHeight = 18.0;
        const double top = (height() - barHeight) / 2.0;
        const bool full = used_ >= capacity_;

        for (int i = 0; i < segments; ++i) {
            const QRectF rect(i * (width + gap), top, width, barHeight);
            QPainterPath path;
            path.addRoundedRect(rect, 4.0, 4.0);
            QColor color;
            if (i < used_) {
                color = full ? QColor(theme::warning()) : QColor(theme::accent());
                if (i == used_ - 1) color = color.darker(112);
            } else {
                color = QColor(theme::separator());
            }
            painter.fillPath(path, color);
        }
    }

private:
    int used_{0};
    int capacity_{20};
};

}  // namespace

// 对外暴露的轻量转发，避免把内部类写进头文件。
class SegmentBar : public QWidget {
public:
    explicit SegmentBar(QWidget* parent = nullptr) : QWidget(parent) {
        inner_ = new SegmentBarWidget(this);
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(inner_);
    }

    void setState(int used, int capacity) { inner_->setState(used, capacity); }

private:
    SegmentBarWidget* inner_{nullptr};
};

OverviewPage::OverviewPage(AppContext* context, QWidget* parent)
    : QWidget(parent), context_(context) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    stack_ = new QStackedWidget(this);
    stack_->addWidget(buildContent());
    stack_->addWidget(buildEmpty());
    root->addWidget(stack_);

    connect(context_, &AppContext::memoriesChanged, this, &OverviewPage::refresh);
    connect(context_, &AppContext::graphChanged, this, &OverviewPage::refresh);
    refresh();
}

QWidget* OverviewPage::buildContent() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);

    // 第一行：四张指标卡
    auto* kpiRow = new QHBoxLayout();
    kpiRow->setSpacing(14);
    memoryCard_ = new KpiCard(QStringLiteral("长期记忆总数"), QStringLiteral("▤"), page);
    memoryCard_->setAccent(QColor(theme::accent()));
    shortTermCard_ = new KpiCard(QStringLiteral("短期队列"), QStringLiteral("◔"), page);
    shortTermCard_->setAccent(QColor(theme::success()));
    lruCard_ = new KpiCard(QStringLiteral("LRU 活跃索引"), QStringLiteral("⇄"), page);
    lruCard_->setAccent(QColor("#F2994A"));
    graphCard_ = new KpiCard(QStringLiteral("实体 / 关系"), QStringLiteral("◈"), page);
    graphCard_->setAccent(QColor("#7C5CFC"));
    for (auto* card : {memoryCard_, shortTermCard_, lruCard_, graphCard_}) {
        kpiRow->addWidget(card, 1);
    }
    layout->addLayout(kpiRow);

    // 第二行：最近录入 + 短期队列占用
    auto* middleRow = new QHBoxLayout();
    middleRow->setSpacing(14);

    recentList_ = new QListWidget(page);
    recentList_->setObjectName(QStringLiteral("recentList"));
    recentList_->setFrameShape(QFrame::NoFrame);
    connect(recentList_, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
        if (item != nullptr) emit openMemoryRequested(item->data(Qt::UserRole).toString());
    });
    middleRow->addWidget(makeCard(QStringLiteral("最近录入（点击跳转记忆管理）"), recentList_, page), 1);

    auto* queueBody = new QWidget(page);
    auto* queueLayout = new QVBoxLayout(queueBody);
    queueLayout->setContentsMargins(0, 0, 0, 0);
    queueLayout->setSpacing(10);
    segmentBar_ = new SegmentBar(queueBody);
    queueLayout->addWidget(segmentBar_);
    recentHintLabel_ = new QLabel(queueBody);
    recentHintLabel_->setWordWrap(true);
    queueLayout->addWidget(recentHintLabel_);
    evictedLabel_ = new QLabel(queueBody);
    evictedLabel_->setWordWrap(true);
    evictedLabel_->setStyleSheet(
        QStringLiteral("QLabel { color: %1; }").arg(theme::textSecondary()));
    queueLayout->addWidget(evictedLabel_);
    queueLayout->addStretch(1);
    middleRow->addWidget(makeCard(QStringLiteral("短期队列占用"), queueBody, page), 1);
    layout->addLayout(middleRow, 1);

    // 第三行：类型分布 + 重要度分布
    auto* chartRow = new QHBoxLayout();
    chartRow->setSpacing(14);
    typeChart_ = new TypePieChart(page);
    typeChart_->setMinimumHeight(220);
    chartRow->addWidget(makeCard(QStringLiteral("记忆类型分布"), typeChart_, page), 1);

    importanceChart_ = new QChartView(page);
    importanceChart_->setRenderHint(QPainter::Antialiasing, true);
    importanceChart_->setBackgroundBrush(Qt::NoBrush);
    importanceChart_->setFrameShape(QFrame::NoFrame);
    importanceChart_->setStyleSheet(QStringLiteral("background: transparent;"));
    importanceChart_->viewport()->setAutoFillBackground(false);
    importanceChart_->setMinimumHeight(220);
    chartRow->addWidget(makeCard(QStringLiteral("重要度分布（1–5）"), importanceChart_, page), 1);
    layout->addLayout(chartRow, 1);

    return page;
}

QWidget* OverviewPage::buildEmpty() {
    emptyState_ = new EmptyState(this);
    emptyState_->setText(QStringLiteral("还没有任何记忆"),
                         QStringLiteral("点击下面的按钮录入第一条记忆，左侧导航也可以开始演示。"));
    emptyState_->setActionText(QStringLiteral("录入第一条记忆"));
    emptyState_->setActionVisible(true);
    connect(emptyState_, &EmptyState::actionTriggered, this,
            &OverviewPage::createMemoryRequested);
    return emptyState_;
}

void OverviewPage::refresh() {
    if (context_ == nullptr) return;
    const auto& manager = context_->manager();
    const auto& graph = context_->graph();

    if (manager.size() == 0) {
        stack_->setCurrentIndex(1);
        return;
    }
    stack_->setCurrentIndex(0);

    memoryCard_->setValue(QString::number(manager.size()));
    memoryCard_->setSubtitle(QStringLiteral("哈希桶 %1 个 · 负载因子 %2")
                                 .arg(manager.bucketCount())
                                 .arg(fmt::number(manager.hashLoadFactor(), 3)));

    shortTermCard_->setValue(QStringLiteral("%1 / %2")
                                 .arg(manager.shortTermSize())
                                 .arg(manager.shortTermCapacity()));
    shortTermCard_->setSubtitle(manager.shortTermSize() >= manager.shortTermCapacity()
                                    ? QStringLiteral("已写满，新记忆会淘汰最旧条目")
                                    : QStringLiteral("剩余 %1 个位置")
                                          .arg(manager.shortTermCapacity() -
                                               manager.shortTermSize()));

    lruCard_->setValue(QStringLiteral("%1 / %2")
                           .arg(manager.lruSize())
                           .arg(manager.lruCapacity()));
    const auto lruOrder = manager.lruOrder();
    lruCard_->setSubtitle(lruOrder.empty()
                              ? QStringLiteral("暂无访问记录")
                              : QStringLiteral("最近访问：%1")
                                    .arg(QString::fromStdString(lruOrder.front())));

    graphCard_->setValue(QStringLiteral("%1 / %2")
                             .arg(graph.activeEntityCount())
                             .arg(graph.activeEdgeCount()));
    graphCard_->setSubtitle(QStringLiteral("含失效项共 %1 实体、%2 关系")
                                .arg(graph.entityCount())
                                .arg(graph.edgeCount()));

    // 最近录入
    recentList_->clear();
    auto memories = manager.all();
    std::sort(memories.begin(), memories.end(), [](const auto& left, const auto& right) {
        if (left.createdAt != right.createdAt) return left.createdAt > right.createdAt;
        return left.id < right.id;
    });
    const int limit = qMin<int>(5, static_cast<int>(memories.size()));
    for (int i = 0; i < limit; ++i) {
        const auto& value = memories[static_cast<std::size_t>(i)];
        auto* item = new QListWidgetItem(
            QStringLiteral("%1  %2\n%3")
                .arg(fmt::timeText(value.createdAt),
                     theme::memoryTypeLabel(value.type),
                     fmt::shorten(QString::fromStdString(value.content), 34)));
        item->setData(Qt::UserRole, QString::fromStdString(value.id));
        item->setForeground(QColor(theme::textPrimary()));
        recentList_->addItem(item);
    }

    // 短期队列
    const auto recentIds = manager.recentIds();
    segmentBar_->setState(static_cast<int>(recentIds.size()),
                          static_cast<int>(manager.shortTermCapacity()));
    recentHintLabel_->setText(
        QStringLiteral("队列头 %1　队列尾 %2")
            .arg(recentIds.empty() ? QStringLiteral("—")
                                   : QString::fromStdString(recentIds.front()),
                 recentIds.empty() ? QStringLiteral("—")
                                   : QString::fromStdString(recentIds.back())));
    QStringList evicted;
    for (auto it = recentIds.rbegin(); it != recentIds.rend() && evicted.size() < 3; ++it) {
        if (manager.peek(*it) == nullptr) {
            evicted << QString::fromStdString(*it) + QStringLiteral("（已删除）");
        }
    }
    evictedLabel_->setText(
        evicted.isEmpty()
            ? QStringLiteral("容量 %1：写满后每新增一条记忆，最旧的一条会被覆盖（长期记忆保留）。")
                  .arg(manager.shortTermCapacity())
            : QStringLiteral("短期队列中的已删除残留 ID：%1").arg(evicted.join(QStringLiteral("、"))));

    // 类型分布
    QHash<int, int> typeCounts;
    QHash<int, int> importanceCounts;
    for (const auto& value : memories) {
        typeCounts[static_cast<int>(value.type)] += 1;
        importanceCounts[value.importance] += 1;
    }
    QVector<TypePieChart::Slice> slices;
    for (auto type : {memory::MemoryType::Profile, memory::MemoryType::Plan,
                      memory::MemoryType::Conversation, memory::MemoryType::Event,
                      memory::MemoryType::Other}) {
        const int count = typeCounts.value(static_cast<int>(type), 0);
        if (count <= 0) continue;
        slices.push_back({theme::memoryTypeLabel(type), count, theme::memoryTypeColor(type)});
    }
    typeChart_->setSlices(slices);

    // 重要度分布
    auto* barSet = new QBarSet(QStringLiteral("记忆条数"));
    barSet->setColor(QColor(theme::accent()));
    QStringList categories;
    int maxCount = 0;
    for (int level = 1; level <= 5; ++level) {
        const int count = importanceCounts.value(level, 0);
        *barSet << count;
        categories << QStringLiteral("L%1").arg(level);
        maxCount = qMax(maxCount, count);
    }
    auto* series = new QBarSeries();
    series->append(barSet);
    auto* chart = new QChart();
    chart->addSeries(series);
    chart->setBackgroundVisible(false);
    chart->legend()->setVisible(false);
    chart->setMargins(QMargins(4, 4, 4, 4));
    auto* axisX = new QBarCategoryAxis();
    axisX->append(categories);
    axisX->setLabelsColor(QColor(theme::textSecondary()));
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);
    auto* axisY = new QValueAxis();
    axisY->setRange(0, qMax(1, maxCount + 1));
    axisY->setLabelFormat("%d");
    axisY->setLabelsColor(QColor(theme::textMuted()));
    axisY->setGridLineColor(QColor(theme::separator()));
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);
    importanceChart_->setChart(chart);
}

}  // namespace gui
