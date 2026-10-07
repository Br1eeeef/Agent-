#include "dialogs/MemoryEditDialog.h"

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

MemoryEditDialog::MemoryEditDialog(AppContext* context, QWidget* parent)
    : QDialog(parent), context_(context) {
    setWindowTitle(QStringLiteral("新增记忆"));
    setMinimumWidth(520);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(12);

    auto* form = new QFormLayout();
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    idEdit_ = new QLineEdit(suggestId(), this);
    idEdit_->setPlaceholderText(QStringLiteral("唯一标识，例如 mem-20261007-120000"));
    form->addRow(QStringLiteral("记忆 ID"), idEdit_);

    contentEdit_ = new QPlainTextEdit(this);
    contentEdit_->setPlaceholderText(QStringLiteral("例如：用户需要重修微积分，并希望讲解定理时带上数学史"));
    contentEdit_->setFixedHeight(96);
    form->addRow(QStringLiteral("记忆内容"), contentEdit_);

    typeCombo_ = new QComboBox(this);
    typeCombo_->addItem(theme::memoryTypeLabel(memory::MemoryType::Profile),
                        static_cast<int>(memory::MemoryType::Profile));
    typeCombo_->addItem(theme::memoryTypeLabel(memory::MemoryType::Plan),
                        static_cast<int>(memory::MemoryType::Plan));
    typeCombo_->addItem(theme::memoryTypeLabel(memory::MemoryType::Conversation),
                        static_cast<int>(memory::MemoryType::Conversation));
    typeCombo_->addItem(theme::memoryTypeLabel(memory::MemoryType::Event),
                        static_cast<int>(memory::MemoryType::Event));
    typeCombo_->addItem(theme::memoryTypeLabel(memory::MemoryType::Other),
                        static_cast<int>(memory::MemoryType::Other));
    form->addRow(QStringLiteral("记忆类型"), typeCombo_);

    auto* importanceRow = new QWidget(this);
    auto* importanceLayout = new QHBoxLayout(importanceRow);
    importanceLayout->setContentsMargins(0, 0, 0, 0);
    importanceSlider_ = new QSlider(Qt::Horizontal, importanceRow);
    importanceSlider_->setRange(1, 5);
    importanceSlider_->setValue(3);
    importanceSlider_->setTickInterval(1);
    importanceSlider_->setTickPosition(QSlider::TicksBelow);
    importanceValue_ = new QLabel(QStringLiteral("3"), importanceRow);
    importanceValue_->setFixedWidth(72);
    importanceLayout->addWidget(importanceSlider_, 1);
    importanceLayout->addWidget(importanceValue_);
    connect(importanceSlider_, &QSlider::valueChanged, this, [this](int value) {
        importanceValue_->setText(QStringLiteral("%1 / 5").arg(value));
        importanceValue_->setStyleSheet(
            QStringLiteral("QLabel { color: %1; font-weight: 600; }")
                .arg(theme::importanceColor(value).name()));
    });
    importanceValue_->setText(QStringLiteral("3 / 5"));
    importanceValue_->setStyleSheet(
        QStringLiteral("QLabel { color: %1; font-weight: 600; }")
            .arg(theme::importanceColor(3).name()));
    form->addRow(QStringLiteral("重要度"), importanceRow);

    keywordsEdit_ = new QLineEdit(this);
    keywordsEdit_->setPlaceholderText(QStringLiteral("逗号分隔；留空则由核心自动分词"));
    form->addRow(QStringLiteral("关键词"), keywordsEdit_);

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
    connect(buttons, &QDialogButtonBox::accepted, this, &MemoryEditDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

QString MemoryEditDialog::suggestId() const {
    return QStringLiteral("mem-%1").arg(
        QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")));
}

void MemoryEditDialog::loadForEdit(const memory::Memory& value) {
    editMode_ = true;
    setWindowTitle(QStringLiteral("编辑记忆"));
    idEdit_->setText(QString::fromStdString(value.id));
    idEdit_->setEnabled(false);
    contentEdit_->setPlainText(QString::fromStdString(value.content));
    const int typeIndex = typeCombo_->findData(static_cast<int>(value.type));
    if (typeIndex >= 0) typeCombo_->setCurrentIndex(typeIndex);
    importanceSlider_->setValue(value.importance);
    QStringList keywords;
    for (const auto& keyword : value.keywords) {
        keywords << QString::fromStdString(keyword);
    }
    keywordsEdit_->setText(keywords.join(QStringLiteral(", ")));
}

std::vector<std::string> MemoryEditDialog::keywords() const {
    std::vector<std::string> result;
    const QStringList parts = keywordsEdit_->text().split(
        QRegularExpression(QStringLiteral("[,，;；\\s]+")), Qt::SkipEmptyParts);
    for (const auto& part : parts) result.push_back(part.trimmed().toStdString());
    return result;
}

QString MemoryEditDialog::id() const { return idEdit_->text().trimmed(); }

memory::Memory MemoryEditDialog::toMemory() const {
    const auto type = static_cast<memory::MemoryType>(typeCombo_->currentData().toInt());
    return memory::Memory::create(idEdit_->text().trimmed().toStdString(),
                                  contentEdit_->toPlainText().toStdString(),
                                  importanceSlider_->value(), type, keywords());
}

void MemoryEditDialog::setError(const QString& text) {
    if (text.isEmpty()) {
        errorLabel_->hide();
        return;
    }
    errorLabel_->setText(text);
    errorLabel_->show();
}

void MemoryEditDialog::accept() {
    setError(QString());
    const QString idText = idEdit_->text().trimmed();
    if (idText.isEmpty()) {
        setError(QStringLiteral("记忆 ID 不能为空。"));
        return;
    }
    if (contentEdit_->toPlainText().trimmed().isEmpty()) {
        setError(QStringLiteral("记忆内容不能为空。"));
        return;
    }
    if (!editMode_ && context_ != nullptr) {
        if (context_->manager().peek(idText.toStdString()) != nullptr) {
            setError(QStringLiteral("ID %1 已存在，请换一个。").arg(idText));
            return;
        }
    }
    try {
        (void)toMemory();  // 复用核心校验，异常统一在内联错误条中呈现
    } catch (const std::exception& ex) {
        setError(QString::fromUtf8(ex.what()));
        return;
    }
    QDialog::accept();
}

}  // namespace gui
