#pragma once

#include <QColor>
#include <QLabel>
#include <QString>

namespace gui {

// 统一的胶囊标签：圆角 999px，填充色为语义色的 12% 透明。
class ChipLabel : public QLabel {
    Q_OBJECT

public:
    explicit ChipLabel(QWidget* parent = nullptr);
    ChipLabel(const QString& text, const QColor& color, QWidget* parent = nullptr);

    void setChip(const QString& text, const QColor& color);
    void setOutlined(bool outlined);

private:
    void restyle();

    QString text_;
    QColor color_{"#4F6BED"};
    bool outlined_{false};
};

}  // namespace gui
