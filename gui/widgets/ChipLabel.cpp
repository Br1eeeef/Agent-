#include "widgets/ChipLabel.h"

#include "theme/Theme.h"

namespace gui {

ChipLabel::ChipLabel(QWidget* parent) : QLabel(parent) { restyle(); }

ChipLabel::ChipLabel(const QString& text, const QColor& color, QWidget* parent)
    : QLabel(parent) {
    setChip(text, color);
}

void ChipLabel::setChip(const QString& text, const QColor& color) {
    text_ = text;
    color_ = color;
    setText(text_);
    restyle();
}

void ChipLabel::setOutlined(bool outlined) {
    outlined_ = outlined;
    restyle();
}

void ChipLabel::restyle() {
    const QString fill = outlined_ ? QStringLiteral("transparent")
                                   : theme::alpha(color_.name(), 0.12);
    setAlignment(Qt::AlignCenter);
    setStyleSheet(QStringLiteral(
                      "QLabel {"
                      "  color: %1;"
                      "  background: %2;"
                      "  border: 1px solid %3;"
                      "  border-radius: 999px;"
                      "  padding: 2px 10px;"
                      "  font-size: 9pt;"
                      "}")
                      .arg(color_.name(), fill, theme::alpha(color_.name(), 0.32)));
}

}  // namespace gui
