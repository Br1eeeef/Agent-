#include "widgets/Toast.h"

#include "theme/Theme.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>

#include <algorithm>

namespace gui {

std::vector<Toast*>& Toast::activeToasts() {
    static std::vector<Toast*> toasts;
    return toasts;
}

Toast::Toast(QWidget* parent, const QString& text, bool error)
    : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setObjectName(QStringLiteral("toast"));

    const QColor accent = error ? QColor(theme::danger()) : QColor(theme::success());
    setStyleSheet(QStringLiteral(
                      "QWidget#toast {"
                      "  background: %1;"
                      "  border: 1px solid %2;"
                      "  border-left: 4px solid %3;"
                      "  border-radius: 8px;"
                      "}")
                      .arg(theme::surface(), theme::border(), accent.name()));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 10, 14, 10);
    label_ = new QLabel(text, this);
    label_->setWordWrap(true);
    label_->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                              .arg(error ? theme::danger() : theme::textPrimary()));
    layout->addWidget(label_);

    adjustSize();
    setFixedWidth(qMin(360, qMax(220, sizeHint().width())));

    timer_ = new QTimer(this);
    timer_->setSingleShot(true);
    connect(timer_, &QTimer::timeout, this, &Toast::dismiss);
}

void Toast::show(QWidget* parent, const QString& text, bool error, int milliseconds) {
    if (parent == nullptr) return;
    auto* toast = new Toast(parent, text, error);
    // 必须先登记再摆位：restack() 只处理已经在册的提示，否则新提示会留在 (0,0)，
    // 也就是窗口左上角。
    activeToasts().push_back(toast);
    // 类内的静态 show() 会隐藏 QWidget::show()，这里显式调用基类版本。
    toast->QWidget::show();
    toast->raise();
    toast->restack();
    toast->timer_->start(milliseconds);
}

void Toast::dismiss() {
    auto& toasts = activeToasts();
    toasts.erase(std::remove(toasts.begin(), toasts.end(), this), toasts.end());
    for (auto* toast : toasts) toast->restack();
    deleteLater();
}

void Toast::restack() {
    QWidget* parent = parentWidget();
    if (parent == nullptr) return;
    const int margin = 18;
    int bottom = parent->height() - margin;
    const auto& toasts = activeToasts();
    // 从最后一条（最新）开始向上堆叠。
    for (auto it = toasts.rbegin(); it != toasts.rend(); ++it) {
        Toast* toast = *it;
        if (toast->parentWidget() != parent) continue;
        toast->move(parent->width() - toast->width() - margin,
                    bottom - toast->height());
        bottom -= toast->height() + 8;
    }
}

}  // namespace gui
