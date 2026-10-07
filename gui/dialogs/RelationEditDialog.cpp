#include "dialogs/RelationEditDialog.h"

#include "theme/Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

namespace gui {

RelationEditDialog::RelationEditDialog(AppContext* context, QWidget* parent)
    : QDialog(parent), context_(context) {
    setWindowTitle(QStringLiteral("新增关系"));
    setMinimumWidth(520);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(12);

    auto* form = new QFormLayout();
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    subjectCombo_ = new QComboBox(this);
    subjectCombo_->setEditable(true);
    subjectCombo_->setInsertPolicy(QComboBox::NoInsert);
    objectCombo_ = new QComboBox(this);
    objectCombo_->setEditable(true);
    objectCombo_->setInsertPolicy(QComboBox::NoInsert);
    addEndpointItems();

    form->addRow(QStringLiteral("起点实体"), subjectCombo_);
    form->addRow(QStringLiteral("终点实体"), objectCombo_);

    relationEdit_ = new QLineEdit(this);
    relationEdit_->setPlaceholderText(QStringLiteral("例如：涉及 / 属于 / 适用于"));
    form->addRow(QStringLiteral("关系名称"), relationEdit_);

    descriptionEdit_ = new QLineEdit(this);
    descriptionEdit_->setPlaceholderText(QStringLiteral("可选，补充说明"));
    form->addRow(QStringLiteral("描述"), descriptionEdit_);

    sourceCombo_ = new QComboBox(this);
    sourceCombo_->addItem(theme::edgeSourceLabel(memory::EdgeSource::User),
                          static_cast<int>(memory::EdgeSource::User));
    sourceCombo_->addItem(theme::edgeSourceLabel(memory::EdgeSource::Agent),
                          static_cast<int>(memory::EdgeSource::Agent));
    sourceCombo_->addItem(theme::edgeSourceLabel(memory::EdgeSource::Extractor),
                          static_cast<int>(memory::EdgeSource::Extractor));
    sourceCombo_->addItem(theme::edgeSourceLabel(memory::EdgeSource::Cooccurrence),
                          static_cast<int>(memory::EdgeSource::Cooccurrence));
    form->addRow(QStringLiteral("来源"), sourceCombo_);

    auto* priorityRow = new QWidget(this);
    auto* priorityLayout = new QHBoxLayout(priorityRow);
    priorityLayout->setContentsMargins(0, 0, 0, 0);
    prioritySlider_ = new QSlider(Qt::Horizontal, priorityRow);
    prioritySlider_->setRange(1, 5);
    prioritySlider_->setValue(3);
    priorityValue_ = new QLabel(QStringLiteral("3 / 5"), priorityRow);
    priorityValue_->setFixedWidth(72);
    priorityLayout->addWidget(prioritySlider_, 1);
    priorityLayout->addWidget(priorityValue_);
    connect(prioritySlider_, &QSlider::valueChanged, this, [this](int value) {
        priorityValue_->setText(QStringLiteral("%1 / 5").arg(value));
    });
    form->addRow(QStringLiteral("优先级"), priorityRow);

    confidenceSpin_ = new QDoubleSpinBox(this);
    confidenceSpin_->setRange(0.0, 1.0);
    confidenceSpin_->setSingleStep(0.05);
    confidenceSpin_->setDecimals(2);
    confidenceSpin_->setValue(1.0);
    form->addRow(QStringLiteral("置信度"), confidenceSpin_);

    auto* timeRow = new QWidget(this);
    auto* timeLayout = new QHBoxLayout(timeRow);
    timeLayout->setContentsMargins(0, 0, 0, 0);
    useNowCheck_ = new QCheckBox(QStringLiteral("使用当前时间"), timeRow);
    useNowCheck_->setChecked(true);
    eventTimeEdit_ = new QDateTimeEdit(QDateTime::currentDateTime(), timeRow);
    eventTimeEdit_->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm"));
    eventTimeEdit_->setEnabled(false);
    timeLayout->addWidget(useNowCheck_);
    timeLayout->addWidget(eventTimeEdit_, 1);
    connect(useNowCheck_, &QCheckBox::toggled, this,
            [this](bool checked) { eventTimeEdit_->setEnabled(!checked); });
    form->addRow(QStringLiteral("事件时间"), timeRow);

    layout->addLayout(form);

    hintLabel_ = new QLabel(
        QStringLiteral("提示：端点名称不存在时会自动创建占位实体，可稍后在图上右键合并。"), this);
    hintLabel_->setWordWrap(true);
    hintLabel_->setStyleSheet(QStringLiteral("QLabel { color: %1; }").arg(theme::textMuted()));
    layout->addWidget(hintLabel_);

    errorLabel_ = new QLabel(this);
    errorLabel_->setWordWrap(true);
    errorLabel_->setStyleSheet(
        QStringLiteral("QLabel { color: %1; background: %2; border: 1px solid %3;"
                       " border-radius: 6px; padding: 6px 10px; }")
            .arg(theme::danger(), theme::alpha(theme::danger(), 0.08),
                 theme::alpha(theme::danger(), 0.28)));
    errorLabel_->hide();
    layout->addWidget(errorLabel_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("保存"));
    buttons->button(QDialogButtonBox::Ok)->setProperty("variant", "primary");
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    connect(buttons, &QDialogButtonBox::accepted, this, &RelationEditDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void RelationEditDialog::addEndpointItems() {
    if (context_ == nullptr) return;
    for (const auto& entity : context_->graph().entities()) {
        if (!entity.active) continue;
        const QString name = QString::fromStdString(entity.name);
        subjectCombo_->addItem(name);
        objectCombo_->addItem(name);
    }
}

void RelationEditDialog::prefill(const QString& subject, const QString& object) {
    if (!subject.isEmpty()) subjectCombo_->setCurrentText(subject);
    if (!object.isEmpty()) objectCombo_->setCurrentText(object);
}

RelationEditDialog::Result RelationEditDialog::result() const {
    Result value;
    value.subject = subjectCombo_->currentText().trimmed();
    value.object = objectCombo_->currentText().trimmed();
    value.relation = relationEdit_->text().trimmed();
    value.description = descriptionEdit_->text().trimmed();
    value.source = static_cast<memory::EdgeSource>(sourceCombo_->currentData().toInt());
    value.priority = prioritySlider_->value();
    value.confidence = confidenceSpin_->value();
    value.eventTime = useNowCheck_->isChecked()
                          ? 0
                          : eventTimeEdit_->dateTime().toSecsSinceEpoch();
    return value;
}

void RelationEditDialog::setError(const QString& text) {
    if (text.isEmpty()) {
        errorLabel_->hide();
        return;
    }
    errorLabel_->setText(text);
    errorLabel_->show();
}

void RelationEditDialog::accept() {
    setError(QString());
    const Result value = result();
    if (value.subject.isEmpty() || value.object.isEmpty()) {
        setError(QStringLiteral("起点与终点实体都不能为空。"));
        return;
    }
    if (value.subject == value.object) {
        setError(QStringLiteral("起点与终点不能是同一个实体。"));
        return;
    }
    if (value.relation.isEmpty()) {
        setError(QStringLiteral("关系名称不能为空。"));
        return;
    }
    QDialog::accept();
}

}  // namespace gui
