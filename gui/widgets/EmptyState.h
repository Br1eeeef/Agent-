#pragma once

#include <QString>
#include <QWidget>

class QPushButton;

namespace gui {

// 空数据引导态：内联插画 + 标题 + 说明 + 可选操作按钮。
class EmptyState : public QWidget {
    Q_OBJECT

public:
    explicit EmptyState(QWidget* parent = nullptr);

    void setText(const QString& title, const QString& subtitle);
    void setActionText(const QString& text);
    void setActionVisible(bool visible);

signals:
    void actionTriggered();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString title_;
    QString subtitle_;
    QPushButton* action_{nullptr};
};

}  // namespace gui
