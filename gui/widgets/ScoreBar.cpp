#include "widgets/ScoreBar.h"

#include "theme/Theme.h"

#include <QPainter>
#include <QPainterPath>

namespace gui {

ScoreBar::ScoreBar(const QString& label, QWidget* parent)
    : QWidget(parent), label_(label) {
    setMinimumHeight(20);
}

void ScoreBar::setValue(double value, const QString& text) {
    value_ = qBound(0.0, value, 1.0);
    text_ = text.isEmpty() ? QString::number(value_, 'f', 3) : text;
    update();
}

void ScoreBar::setBarColor(const QColor& color) {
    color_ = color;
    update();
}

void ScoreBar::setLabelWidth(int width) {
    labelWidth_ = width;
    update();
}

QSize ScoreBar::sizeHint() const { return QSize(240, 20); }

void ScoreBar::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const int valueWidth = 52;
    const int gap = 8;
    const QRect labelRect(0, 0, labelWidth_, height());
    const int trackLeft = labelWidth_ + gap;
    const int trackWidth = qMax(24, width() - trackLeft - valueWidth - gap);
    const QRect trackRect(trackLeft, (height() - 6) / 2, trackWidth, 6);
    const QRect valueRect(width() - valueWidth, 0, valueWidth, height());

    QFont small = theme::baseFont(9);
    painter.setFont(small);
    painter.setPen(QColor(theme::textSecondary()));
    painter.drawText(labelRect, Qt::AlignVCenter | Qt::AlignLeft, label_);

    QPainterPath track;
    track.addRoundedRect(QRectF(trackRect), 3.0, 3.0);
    painter.fillPath(track, QColor(theme::separator()));

    const double filled = trackWidth * value_;
    if (filled > 0.5) {
        QPainterPath bar;
        bar.addRoundedRect(QRectF(trackRect.left(), trackRect.top(), filled, trackRect.height()),
                           3.0, 3.0);
        painter.fillPath(bar, color_);
    }

    QFont valueFont = theme::baseFont(9);
    valueFont.setBold(true);
    painter.setFont(valueFont);
    painter.setPen(QColor(theme::textPrimary()));
    painter.drawText(valueRect, Qt::AlignVCenter | Qt::AlignRight, text_);
}

}  // namespace gui
