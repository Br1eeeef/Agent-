#pragma once

#include <QColor>
#include <QString>
#include <QVector>
#include <QWidget>

class QChartView;

namespace gui {

// 环形图封装：用于总览页的类型分布。
class TypePieChart : public QWidget {
    Q_OBJECT

public:
    struct Slice {
        QString label;
        int value{0};
        QColor color;
    };

    explicit TypePieChart(QWidget* parent = nullptr);
    void setSlices(const QVector<Slice>& slices);
    void setEmptyText(const QString& text);

private:
    QChartView* chartView_{nullptr};
    QString emptyText_{QStringLiteral("暂无数据")};
};

}  // namespace gui
