#include "dialogs/EntityEditDialog.h"

#include "theme/Theme.h"

#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSlider>
#include <QVBoxLayout>

#include <stdexcept>

namespace gui {

EntityEditDialog::EntityEditDialog(AppContext* context, QWidget* parent)
    : QDialog(parent), context_(context) {
    newId_ = QStringLiteral("ent-%1").arg(
        QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")));
    setWindowTitle(QStringLiteral("新增实体"));
    setMinimumWidth(500);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(12);

    auto* form = new QFormLayout();
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    nameEdit_ = new QLineEdit(this);
    nameEdit_->setPlaceholderText(QStringLiteral("例如：微积分"));
    form->addRow(QStringLiteral("实体名称"), nameEdit_);

    typeCombo_ = new QComboBox(this);
    typeCombo_->addItem(theme::entityTypeLabel(memory::EntityType::Concept),
                        static_cast<int>(memory::EntityType::Concept));
    typeCombo_->addItem(theme::entityTypeLabel(memory::EntityType::Fact),
                        static_cast<int>(memory::EntityType::Fact));
    typeCombo_->addItem(theme::entityTypeLabel(memory::EntityType::Preference),
                        static_cast<int>(memory::EntityType::Preference));
    typeCombo_->addItem(theme::entityTypeLabel(memory::EntityType::Other),
                        static_cast<int>(memory::EntityType::Other));
    form->addRow(QStringLiteral("实体类型"), typeCombo_);

    aliasEdit_ = new QLineEdit(this);
    aliasEdit_->setPlaceholderText(QStringLiteral("逗号分隔，例如：高等数学, calculus"));
    form->addRow(QStringLiteral("别名"), aliasEdit_);

    descriptionEdit_ = new QPlainTextEdit(this);
    descriptionEdit_->setFixedHeight(72);
    descriptionEdit_->setPlaceholderText(QStringLiteral("补充说明，可留空"));
    form->addRow(QStringLiteral("描述"), descriptionEdit_);

    auto* priorityRow = new QWidget(this);
    auto* priorityLayout = new QHBoxLayout(priorityRow);
    priorityLayout->setContentsMargins(0, 0, 0, 0);
    prioritySlider_ = new QSlider(Qt::Horizontal, priorityRow);
    prioritySlider_->setRange(1, 5);
    prioritySlider_->setValue(3);
    prioritySlider_->setTickInterval(1);
    prioritySlider_->setTickPosition(QSlider::TicksBelow);
    priorityValue_ = new QLabel(QStringLiteral("3 / 5"), priorityRow);
    priorityValue_->setFixedWidth(72);
    priorityLayout->addWidget(prioritySlider_, 1);
    priorityLayout->addWidget(priorityValue_);
    connect(prioritySlider_, &QSlider::valueChanged, this, [this](int value) {
        priorityValue_->setText(QStringLiteral("%1 / 5").arg(value));
        priorityValue_->setStyleSheet(
            QStringLiteral("QLabel { color: %1; font-weight: 600; }")
                .arg(theme::importanceColor(value).name()));
    });
    priorityValue_->setStyleSheet(
        QStringLiteral("QLabel { color: %1; font-weight: 600; }")
            .arg(theme::importanceColor(3).name()));
    form->addRow(QStringLiteral("优先级"), priorityRow);

    layout->addLayout(form);

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
    connect(buttons, &QDialogButtonBox::accepted, this, &EntityEditDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void EntityEditDialog::loadForEdit(const memory::Entity& entity) {
    editMode_ = true;
    editId_ = QString::fromStdString(entity.id);
    setWindowTitle(QStringLiteral("编辑实体"));
    nameEdit_->setText(QString::fromStdString(entity.name));
    const int typeIndex = typeCombo_->findData(static_cast<int>(entity.type));
    if (typeIndex >= 0) typeCombo_->setCurrentIndex(typeIndex);
    QStringList aliases;
    for (const auto& alias : entity.aliases) aliases << QString::fromStdString(alias);
    aliasEdit_->setText(aliases.join(QStringLiteral(", ")));
    descriptionEdit_->setPlainText(QString::fromStdString(entity.description));
    prioritySlider_->setValue(entity.priority);
}

QString EntityEditDialog::entityName() const { return nameEdit_->text().trimmed(); }

memory::EntityType EntityEditDialog::entityType() const {
    return static_cast<memory::EntityType>(typeCombo_->currentData().toInt());
}

std::vector<std::string> EntityEditDialog::aliases() const {
    std::vector<std::string> result;
    const QStringList parts = aliasEdit_->text().split(
        QRegularExpression(QStringLiteral("[,，;；]+")), Qt::SkipEmptyParts);
    for (const auto& part : parts) result.push_back(part.trimmed().toStdString());
    return result;
}

QString EntityEditDialog::description() const {
    return descriptionEdit_->toPlainText().trimmed();
}

int EntityEditDialog::priority() const { return prioritySlider_->value(); }

memory::Entity EntityEditDialog::toNewEntity() const {
    return memory::Entity::create(newId_.toStdString(), entityName().toStdString(), entityType(),
                                  aliases(), description().toStdString());
}

void EntityEditDialog::setError(const QString& text) {
    if (text.isEmpty()) {
        errorLabel_->hide();
        return;
    }
    errorLabel_->setText(text);
    errorLabel_->show();
}

void EntityEditDialog::accept() {
    setError(QString());
    if (entityName().isEmpty()) {
        setError(QStringLiteral("实体名称不能为空。"));
        return;
    }
    if (description() == QStringLiteral("placeholder")) {
        setError(QStringLiteral("描述不能使用保留值 placeholder。"));
        return;
    }
    QDialog::accept();
}

}  // namespace gui
