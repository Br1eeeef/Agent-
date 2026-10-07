#include "widgets/KpiCard.h"

#include "theme/Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace gui {

KpiCard::KpiCard(const QString& title, const QString& glyph, QWidget* parent)
    : QFrame(parent), title_(title), glyph_(glyph) {
    setObjectName(QStringLiteral("kpiCard"));
    setProperty("card", true);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(14);

    glyphLabel_ = new QLabel(glyph_, this);
    glyphLabel_->setFixedSize(40, 40);
    glyphLabel_->setAlignment(Qt::AlignCenter);
    layout->addWidget(glyphLabel_, 0, Qt::AlignTop);

    auto* textColumn = new QVBoxLayout();
    textColumn->setSpacing(2);

    titleLabel_ = new QLabel(title_, this);
    titleLabel_->setObjectName(QStringLiteral("kpiTitle"));

    valueLabel_ = new QLabel(QStringLiteral("—"), this);
    valueLabel_->setObjectName(QStringLiteral("kpiValue"));

    subtitleLabel_ = new QLabel(QStringLiteral("等待数据"), this);
    subtitleLabel_->setObjectName(QStringLiteral("kpiSubtitle"));
    subtitleLabel_->setWordWrap(true);

    textColumn->addWidget(titleLabel_);
    textColumn->addWidget(valueLabel_);
    textColumn->addWidget(subtitleLabel_);
    layout->addLayout(textColumn, 1);

    setAccent(accent_);
}

void KpiCard::setValue(const QString& value) { valueLabel_->setText(value); }

void KpiCard::setSubtitle(const QString& subtitle) { subtitleLabel_->setText(subtitle); }

void KpiCard::setAccent(const QColor& color) {
    accent_ = color;
    glyphLabel_->setStyleSheet(QStringLiteral(
                                   "QLabel {"
                                   "  color: %1;"
                                   "  background: %2;"
                                   "  border-radius: 10px;"
                                   "  font-size: 16pt;"
                                   "}")
                                   .arg(color.name(), theme::alpha(color.name(), 0.12)));
    valueLabel_->setStyleSheet(
        QStringLiteral("QLabel { color: %1; }").arg(theme::textPrimary()));
}

QString KpiCard::value() const { return valueLabel_->text(); }

QString KpiCard::subtitle() const { return subtitleLabel_->text(); }

}  // namespace gui
