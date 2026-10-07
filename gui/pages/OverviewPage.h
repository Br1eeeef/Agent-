#pragma once

#include "AppContext.h"

#include <QWidget>

class QLabel;
class QListWidget;
class QStackedWidget;
class QChartView;

namespace gui {

class EmptyState;
class KpiCard;
class SegmentBar;
class TypePieChart;

// 总览页：四张指标卡 + 最近录入 + 短期队列占用 + 类型/重要度分布。
class OverviewPage : public QWidget {
    Q_OBJECT

public:
    explicit OverviewPage(AppContext* context, QWidget* parent = nullptr);

    void refresh();

signals:
    void openMemoryRequested(const QString& id);
    void createMemoryRequested();

private:
    QWidget* buildContent();
    QWidget* buildEmpty();

    AppContext* context_{nullptr};
    QStackedWidget* stack_{nullptr};

    KpiCard* memoryCard_{nullptr};
    KpiCard* shortTermCard_{nullptr};
    KpiCard* lruCard_{nullptr};
    KpiCard* graphCard_{nullptr};

    QListWidget* recentList_{nullptr};
    SegmentBar* segmentBar_{nullptr};
    QLabel* evictedLabel_{nullptr};
    QLabel* recentHintLabel_{nullptr};

    TypePieChart* typeChart_{nullptr};
    QChartView* importanceChart_{nullptr};
    EmptyState* emptyState_{nullptr};
};

}  // namespace gui
