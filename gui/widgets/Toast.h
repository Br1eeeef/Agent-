#pragma once

#include <QString>
#include <QWidget>

#include <vector>

class QLabel;
class QTimer;

namespace gui {

// 右下角轻提示，3 秒后自动消失；同一父窗口内多条提示纵向堆叠。
class Toast : public QWidget {
    Q_OBJECT

public:
    static void show(QWidget* parent, const QString& text, bool error = false,
                     int milliseconds = 3000);

private:
    Toast(QWidget* parent, const QString& text, bool error);
    void dismiss();
    void restack();
    static std::vector<Toast*>& activeToasts();

    QLabel* label_{nullptr};
    QTimer* timer_{nullptr};
};

}  // namespace gui
