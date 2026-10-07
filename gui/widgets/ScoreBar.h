#pragma once

#include <QColor>
#include <QString>
#include <QWidget>

namespace gui {

// 得分条：左侧标签、中间圆角条形、右侧数值，用于召回结果的分项得分展示。
class ScoreBar : public QWidget {
    Q_OBJECT

public:
    explicit ScoreBar(const QString& label, QWidget* parent = nullptr);

    void setValue(double value, const QString& text = QString());
    void setBarColor(const QColor& color);
    void setLabelWidth(int width);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString label_;
    QString text_;
    double value_{0.0};
    QColor color_{"#4F6BED"};
    int labelWidth_{64};
};

}  // namespace gui
