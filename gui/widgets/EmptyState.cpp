#include "widgets/EmptyState.h"

#include "theme/Theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QVBoxLayout>

namespace gui {

EmptyState::EmptyState(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(10);
    layout->addStretch(1);

    action_ = new QPushButton(this);
    action_->setProperty("variant", "primary");
    action_->setCursor(Qt::PointingHandCursor);
    connect(action_, &QPushButton::clicked, this, &EmptyState::actionTriggered);
    layout->addWidget(action_, 0, Qt::AlignHCenter);
    layout->addStretch(2);
    setActionVisible(false);
    setText(QStringLiteral("暂无数据"), QStringLiteral("先录入一条数据再回来看看。"));
}

void EmptyState::setText(const QString& title, const QString& subtitle) {
    title_ = title;
    subtitle_ = subtitle;
    update();
}

void EmptyState::setActionText(const QString& text) {
    action_->setText(text);
    if (text.isEmpty()) setActionVisible(false);
}

void EmptyState::setActionVisible(bool visible) { action_->setVisible(visible); }

void EmptyState::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const int iconSize = 96;
    const QRect iconRect((width() - iconSize) / 2, qMax(8, height() / 2 - 110), iconSize,
                         iconSize);

    // 极简插画：三层同心圆 + 中心点，避免额外图片资源。
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(theme::alpha(theme::accent(), 0.10)));
    painter.drawEllipse(iconRect);
    painter.setBrush(QColor(theme::alpha(theme::accent(), 0.18)));
    painter.drawEllipse(iconRect.adjusted(18, 18, -18, -18));
    painter.setBrush(QColor(theme::accent()));
    painter.drawEllipse(iconRect.center(), 8, 8);

    QFont titleFont = theme::baseFont(13, true);
    painter.setFont(titleFont);
    painter.setPen(QColor(theme::textPrimary()));
    const QRect titleRect(0, iconRect.bottom() + 18, width(), 26);
    painter.drawText(titleRect, Qt::AlignHCenter | Qt::AlignVCenter, title_);

    QFont bodyFont = theme::baseFont(10);
    painter.setFont(bodyFont);
    painter.setPen(QColor(theme::textSecondary()));
    const QRect bodyRect(0, titleRect.bottom() + 2, width(), 22);
    painter.drawText(bodyRect, Qt::AlignHCenter | Qt::AlignVCenter, subtitle_);
}

}  // namespace gui
