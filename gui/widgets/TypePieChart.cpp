#include "widgets/TypePieChart.h"

#include "theme/Theme.h"

#include <QChart>
#include <QChartView>
#include <QLegend>
#include <QPieSeries>
#include <QPieSlice>
#include <QVBoxLayout>

namespace gui {

TypePieChart::TypePieChart(QWidget* parent) : QWidget(parent) {
    chartView_ = new QChartView(this);
    chartView_->setRenderHint(QPainter::Antialiasing, true);
    chartView_->setBackgroundBrush(Qt::NoBrush);
    chartView_->setFrameShape(QFrame::NoFrame);
    // QGraphicsView 默认会填充 Base 背景，这里彻底透明化，露出卡片底色。
    chartView_->setStyleSheet(QStringLiteral("background: transparent;"));
    chartView_->viewport()->setAutoFillBackground(false);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(chartView_);
    setSlices({});
}

void TypePieChart::setEmptyText(const QString& text) { emptyText_ = text; }

void TypePieChart::setSlices(const QVector<Slice>& slices) {
    auto* series = new QPieSeries();
    series->setHoleSize(0.58);
    series->setPieSize(0.72);
    int total = 0;
    for (const auto& slice : slices) {
        if (slice.value <= 0) continue;
        auto* item = series->append(slice.label, slice.value);
        item->setColor(slice.color);
        item->setBorderColor(QColor(theme::surface()));
        item->setBorderWidth(2);
        item->setLabelVisible(true);
        item->setLabelColor(QColor(theme::textSecondary()));
        item->setLabelPosition(QPieSlice::LabelOutside);
        total += slice.value;
    }

    auto* chart = new QChart();
    chart->addSeries(series);
    chart->setBackgroundVisible(false);
    chart->setMargins(QMargins(0, 0, 0, 0));
    chart->legend()->setVisible(total > 0);
    chart->legend()->setAlignment(Qt::AlignBottom);
    chart->legend()->setLabelColor(QColor(theme::textSecondary()));
    chart->setTitle(total > 0 ? QString()
                              : QStringLiteral("%1").arg(emptyText_));
    chart->setTitleFont(theme::baseFont(10));
    chart->setTitleBrush(QColor(theme::textMuted()));

    chartView_->setChart(chart);
}

}  // namespace gui
