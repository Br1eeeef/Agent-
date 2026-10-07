#pragma once

#include <QColor>
#include <QFrame>
#include <QString>

class QLabel;

namespace gui {

// 总览与系统状态页的指标卡：标题 + 大号数值 + 副标题。
class KpiCard : public QFrame {
    Q_OBJECT

public:
    explicit KpiCard(const QString& title, const QString& glyph, QWidget* parent = nullptr);

    void setValue(const QString& value);
    void setSubtitle(const QString& subtitle);
    void setAccent(const QColor& color);
    QString value() const;
    QString subtitle() const;

private:
    QLabel* glyphLabel_{nullptr};
    QLabel* titleLabel_{nullptr};
    QLabel* valueLabel_{nullptr};
    QLabel* subtitleLabel_{nullptr};
    QColor accent_{"#4F6BED"};
    QString title_;
    QString glyph_;
};

}  // namespace gui
