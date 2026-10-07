#include "dialogs/RememberDialog.h"

#include "Format.h"
#include "theme/Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDateTimeEdit>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <stdexcept>

namespace gui {
namespace {

constexpr int kEntityNameColumn = 0;
constexpr int kEntityTypeColumn = 1;
constexpr int kEntityAliasColumn = 2;
constexpr int kEntityDescriptionColumn = 3;
constexpr int kEntityPriorityColumn = 4;

constexpr int kRelationSubjectColumn = 0;
constexpr int kRelationObjectColumn = 1;
constexpr int kRelationNameColumn = 2;
constexpr int kRelationDescriptionColumn = 3;
constexpr int kRelationSourceColumn = 4;
constexpr int kRelationPriorityColumn = 5;
constexpr int kRelationConfidenceColumn = 6;
constexpr int kRelationTimeColumn = 7;

QComboBox* makeTypeCombo(QWidget* parent, int index) {
    auto* combo = new QComboBox(parent);
    combo->addItem(theme::entityTypeLabel(memory::EntityType::Concept),
                   static_cast<int>(memory::EntityType::Concept));
    combo->addItem(theme::entityTypeLabel(memory::EntityType::Fact),
                   static_cast<int>(memory::EntityType::Fact));
    combo->addItem(theme::entityTypeLabel(memory::EntityType::Preference),
                   static_cast<int>(memory::EntityType::Preference));
    combo->addItem(theme::entityTypeLabel(memory::EntityType::Other),
                   static_cast<int>(memory::EntityType::Other));
    combo->setCurrentIndex(qBound(0, index, 3));
    return combo;
}

QComboBox* makeSourceCombo(QWidget* parent) {
    auto* combo = new QComboBox(parent);
    combo->addItem(theme::edgeSourceLabel(memory::EdgeSource::User),
                   static_cast<int>(memory::EdgeSource::User));
    combo->addItem(theme::edgeSourceLabel(memory::EdgeSource::Agent),
                   static_cast<int>(memory::EdgeSource::Agent));
    combo->addItem(theme::edgeSourceLabel(memory::EdgeSource::Extractor),
                   static_cast<int>(memory::EdgeSource::Extractor));
    combo->addItem(theme::edgeSourceLabel(memory::EdgeSource::Cooccurrence),
                   static_cast<int>(memory::EdgeSource::Cooccurrence));
    return combo;
}

}  // namespace

RememberDialog::RememberDialog(AppContext* context, QWidget* parent)
    : QDialog(parent), context_(context) {
    setWindowTitle(QStringLiteral("高级录入：记忆 + 实体 + 关系"));
    resize(940, 620);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(12);

    auto* header = new QLabel(
        QStringLiteral("手工填写一条记忆，并同时登记它的实体与关系；提交时在一个事务内写入图数据库。"),
        this);
    header->setStyleSheet(QStringLiteral("QLabel { color: %1; }").arg(theme::textSecondary()));
    layout->addWidget(header);

    auto* tabs = new QTabWidget(this);
    buildMemoryTab();
    buildEntityTab();
    buildRelationTab();
    tabs->addTab(contentEdit_->parentWidget(), QStringLiteral("① 记忆"));
    tabs->addTab(entityTable_->parentWidget(), QStringLiteral("② 实体"));
    tabs->addTab(relationTable_->parentWidget(), QStringLiteral("③ 关系"));
    layout->addWidget(tabs, 1);

    memoryOnlyCheck_ = new QCheckBox(QStringLiteral("仅写记忆（不入图，等价于 MemoryManager::add）"), this);
    layout->addWidget(memoryOnlyCheck_);

    errorLabel_ = new QLabel(this);
    errorLabel_->setWordWrap(true);
    errorLabel_->setStyleSheet(
        QStringLiteral("QLabel { color: %1; background: %2; border: 1px solid %3;"
                       " border-radius: 6px; padding: 6px 10px; }")
            .arg(theme::danger(), theme::alpha(theme::danger(), 0.08),
                 theme::alpha(theme::danger(), 0.28)));
    errorLabel_->hide();
    layout->addWidget(errorLabel_);

    resultLabel_ = new QLabel(this);
    resultLabel_->setWordWrap(true);
    resultLabel_->setStyleSheet(
        QStringLiteral("QLabel { color: %1; background: %2; border: 1px solid %3;"
                       " border-radius: 6px; padding: 6px 10px; }")
            .arg(theme::success(), theme::alpha(theme::success(), 0.10),
                 theme::alpha(theme::success(), 0.30)));
    resultLabel_->hide();
    layout->addWidget(resultLabel_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setText(QStringLiteral("关闭"));
    auto* submitButton = new QPushButton(QStringLiteral("提交录入"), this);
    submitButton->setProperty("variant", "primary");
    buttons->addButton(submitButton, QDialogButtonBox::AcceptRole);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(submitButton, &QPushButton::clicked, this, &RememberDialog::submit);
    layout->addWidget(buttons);
}

void RememberDialog::buildMemoryTab() {
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);
    form->setContentsMargins(16, 16, 16, 16);
    form->setSpacing(10);

    idEdit_ = new QLineEdit(page);
    idEdit_->setText(QStringLiteral("mem-%1").arg(
        QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"))));
    form->addRow(QStringLiteral("记忆 ID"), idEdit_);

    contentEdit_ = new QPlainTextEdit(page);
    contentEdit_->setFixedHeight(110);
    contentEdit_->setPlaceholderText(
        QStringLiteral("例如：用户需要重修微积分，并希望讲解定理时带上数学史"));
    form->addRow(QStringLiteral("记忆内容"), contentEdit_);

    typeCombo_ = new QComboBox(page);
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
    typeCombo_->setCurrentIndex(1);
    form->addRow(QStringLiteral("记忆类型"), typeCombo_);

    auto* importanceRow = new QWidget(page);
    auto* importanceLayout = new QHBoxLayout(importanceRow);
    importanceLayout->setContentsMargins(0, 0, 0, 0);
    importanceSlider_ = new QSlider(Qt::Horizontal, importanceRow);
    importanceSlider_->setRange(1, 5);
    importanceSlider_->setValue(4);
    importanceValue_ = new QLabel(QStringLiteral("4 / 5"), importanceRow);
    importanceValue_->setFixedWidth(72);
    importanceLayout->addWidget(importanceSlider_, 1);
    importanceLayout->addWidget(importanceValue_);
    connect(importanceSlider_, &QSlider::valueChanged, this, [this](int value) {
        importanceValue_->setText(QStringLiteral("%1 / 5").arg(value));
    });
    form->addRow(QStringLiteral("重要度"), importanceRow);

    keywordsEdit_ = new QLineEdit(page);
    keywordsEdit_->setPlaceholderText(QStringLiteral("逗号分隔；留空则由核心自动分词"));
    form->addRow(QStringLiteral("关键词"), keywordsEdit_);
}

void RememberDialog::buildEntityTab() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto* toolbar = new QHBoxLayout();
    auto* addButton = new QPushButton(QStringLiteral("添加实体"), page);
    auto* removeButton = new QPushButton(QStringLiteral("删除选中行"), page);
    auto* suggestButton = new QPushButton(QStringLiteral("从内容提候选"), page);
    toolbar->addWidget(addButton);
    toolbar->addWidget(removeButton);
    toolbar->addWidget(suggestButton);
    toolbar->addStretch(1);
    layout->addLayout(toolbar);

    entityTable_ = new QTableWidget(0, 5, page);
    entityTable_->setHorizontalHeaderLabels({QStringLiteral("实体名称"), QStringLiteral("类型"),
                                             QStringLiteral("别名"), QStringLiteral("描述"),
                                             QStringLiteral("优先级")});
    entityTable_->horizontalHeader()->setSectionResizeMode(kEntityNameColumn,
                                                           QHeaderView::ResizeToContents);
    entityTable_->horizontalHeader()->setSectionResizeMode(kEntityAliasColumn,
                                                           QHeaderView::ResizeToContents);
    entityTable_->horizontalHeader()->setSectionResizeMode(kEntityDescriptionColumn,
                                                           QHeaderView::Stretch);
    entityTable_->verticalHeader()->setVisible(false);
    entityTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(entityTable_, 1);

    connect(addButton, &QPushButton::clicked, this, [this]() { addEntityRow(); });
    connect(removeButton, &QPushButton::clicked, this,
            [this]() { removeSelectedRow(entityTable_); });
    connect(suggestButton, &QPushButton::clicked, this, &RememberDialog::suggestCandidates);
    connect(entityTable_, &QTableWidget::cellChanged, this,
            [this](int, int) { refreshEndpointChoices(); });
}

void RememberDialog::buildRelationTab() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto* toolbar = new QHBoxLayout();
    auto* addButton = new QPushButton(QStringLiteral("添加关系"), page);
    auto* removeButton = new QPushButton(QStringLiteral("删除选中行"), page);
    auto* hint = new QLabel(
        QStringLiteral("端点不存在时自动创建占位实体；重复的「起点+关系+终点」会合并证据而不是重复建边。"),
        page);
    hint->setStyleSheet(QStringLiteral("QLabel { color: %1; }").arg(theme::textMuted()));
    toolbar->addWidget(addButton);
    toolbar->addWidget(removeButton);
    toolbar->addStretch(1);
    layout->addLayout(toolbar);
    layout->addWidget(hint);

    relationTable_ = new QTableWidget(0, 8, page);
    relationTable_->setHorizontalHeaderLabels(
        {QStringLiteral("起点"), QStringLiteral("终点"), QStringLiteral("关系"),
         QStringLiteral("描述"), QStringLiteral("来源"), QStringLiteral("优先级"),
         QStringLiteral("置信度"), QStringLiteral("事件时间")});
    relationTable_->horizontalHeader()->setSectionResizeMode(kRelationDescriptionColumn,
                                                             QHeaderView::Stretch);
    relationTable_->verticalHeader()->setVisible(false);
    relationTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(relationTable_, 1);

    connect(addButton, &QPushButton::clicked, this, [this]() { addRelationRow(); });
    connect(removeButton, &QPushButton::clicked, this,
            [this]() { removeSelectedRow(relationTable_); });
}

void RememberDialog::addEntityRow(const QString& name, int typeIndex, const QString& aliases,
                                  const QString& description, int priority) {
    const int row = entityTable_->rowCount();
    entityTable_->insertRow(row);
    entityTable_->setItem(row, kEntityNameColumn, new QTableWidgetItem(name));
    entityTable_->setCellWidget(row, kEntityTypeColumn, makeTypeCombo(entityTable_, typeIndex));
    entityTable_->setItem(row, kEntityAliasColumn, new QTableWidgetItem(aliases));
    entityTable_->setItem(row, kEntityDescriptionColumn, new QTableWidgetItem(description));
    auto* prioritySpin = new QSpinBox(entityTable_);
    prioritySpin->setRange(1, 5);
    prioritySpin->setValue(priority);
    entityTable_->setCellWidget(row, kEntityPriorityColumn, prioritySpin);
    refreshEndpointChoices();
}

void RememberDialog::addRelationRow(const QString& subject, const QString& object,
                                    const QString& relation) {
    const QStringList names = entityNames();
    const int row = relationTable_->rowCount();
    relationTable_->insertRow(row);

    auto* subjectCombo = new QComboBox(relationTable_);
    subjectCombo->setEditable(true);
    subjectCombo->addItems(names);
    subjectCombo->setCurrentText(subject.isEmpty() ? (names.isEmpty() ? QString() : names.first())
                                                   : subject);

    auto* objectCombo = new QComboBox(relationTable_);
    objectCombo->setEditable(true);
    objectCombo->addItems(names);
    objectCombo->setCurrentText(object.isEmpty()
                                    ? (names.size() > 1 ? names.at(1) : QString())
                                    : object);

    relationTable_->setCellWidget(row, kRelationSubjectColumn, subjectCombo);
    relationTable_->setCellWidget(row, kRelationObjectColumn, objectCombo);
    relationTable_->setItem(row, kRelationNameColumn,
                            new QTableWidgetItem(relation.isEmpty() ? QStringLiteral("涉及")
                                                                   : relation));
    relationTable_->setItem(row, kRelationDescriptionColumn, new QTableWidgetItem(QString()));
    relationTable_->setCellWidget(row, kRelationSourceColumn, makeSourceCombo(relationTable_));

    auto* prioritySpin = new QSpinBox(relationTable_);
    prioritySpin->setRange(1, 5);
    prioritySpin->setValue(3);
    relationTable_->setCellWidget(row, kRelationPriorityColumn, prioritySpin);

    auto* confidenceSpin = new QDoubleSpinBox(relationTable_);
    confidenceSpin->setRange(0.0, 1.0);
    confidenceSpin->setSingleStep(0.05);
    confidenceSpin->setDecimals(2);
    confidenceSpin->setValue(1.0);
    relationTable_->setCellWidget(row, kRelationConfidenceColumn, confidenceSpin);

    auto* timeEdit = new QDateTimeEdit(QDateTime::currentDateTime(), relationTable_);
    timeEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm"));
    relationTable_->setCellWidget(row, kRelationTimeColumn, timeEdit);
}

void RememberDialog::removeSelectedRow(QTableWidget* table) {
    const int row = table->currentRow();
    if (row < 0) return;
    table->removeRow(row);
    if (table == entityTable_) refreshEndpointChoices();
}

void RememberDialog::refreshEndpointChoices() {
    if (relationTable_ == nullptr) return;
    const QStringList names = entityNames();
    for (int row = 0; row < relationTable_->rowCount(); ++row) {
        for (int column : {kRelationSubjectColumn, kRelationObjectColumn}) {
            auto* combo = qobject_cast<QComboBox*>(relationTable_->cellWidget(row, column));
            if (combo == nullptr) continue;
            const QString current = combo->currentText();
            QSignalBlocker blocker(combo);
            combo->clear();
            combo->addItems(names);
            if (!current.isEmpty()) combo->setCurrentText(current);
        }
    }
}

QStringList RememberDialog::entityNames() const {
    QStringList names;
    if (entityTable_ == nullptr) return names;
    for (int row = 0; row < entityTable_->rowCount(); ++row) {
        const QTableWidgetItem* item = entityTable_->item(row, kEntityNameColumn);
        if (item == nullptr) continue;
        const QString name = item->text().trimmed();
        if (!name.isEmpty() && !names.contains(name)) names << name;
    }
    return names;
}

void RememberDialog::suggestCandidates() {
    const QString content = contentEdit_->toPlainText().trimmed();
    if (content.isEmpty()) {
        setError(QStringLiteral("请先在「记忆」页签填写内容，再提取候选实体。"));
        return;
    }
    setError(QString());
    // 本地启发式：按标点切分短语，保留长度 2~14 的片段，按出现顺序去重。
    const QStringList parts = content.split(
        QRegularExpression(QStringLiteral("[，。；、,.;\\s]+")), Qt::SkipEmptyParts);
    const QStringList existing = entityNames();
    int added = 0;
    for (const QString& part : parts) {
        const QString phrase = part.trimmed();
        if (phrase.size() < 2 || phrase.size() > 14) continue;
        if (existing.contains(phrase)) continue;
        if (entityNames().contains(phrase)) continue;
        addEntityRow(phrase, 0, QString(), QStringLiteral("由内容启发式提取，请确认类型"), 3);
        ++added;
        if (added >= 6) break;
    }
    setResult(added > 0 ? QStringLiteral("已添加 %1 个候选实体，请在表格中确认类型与别名。").arg(added)
                        : QStringLiteral("没有提取到合适候选，请手动添加实体。"));
}

void RememberDialog::prefillContent(const QString& content) {
    contentEdit_->setPlainText(content);
}

std::vector<std::string> RememberDialog::parseKeywords(const QString& text) const {
    std::vector<std::string> result;
    const QStringList parts = text.split(QRegularExpression(QStringLiteral("[,，;；\\s]+")),
                                         Qt::SkipEmptyParts);
    for (const auto& part : parts) result.push_back(part.trimmed().toStdString());
    return result;
}

void RememberDialog::submit() {
    setError(QString());
    setResult(QString());

    memory::RememberRequest request;
    try {
        const auto type = static_cast<memory::MemoryType>(typeCombo_->currentData().toInt());
        request.memory =
            memory::Memory::create(idEdit_->text().trimmed().toStdString(),
                                   contentEdit_->toPlainText().toStdString(),
                                   importanceSlider_->value(), type,
                                   parseKeywords(keywordsEdit_->text()));
    } catch (const std::exception& ex) {
        setError(QStringLiteral("记忆校验失败：%1").arg(QString::fromUtf8(ex.what())));
        return;
    }

    if (memoryOnlyCheck_->isChecked()) {
        QString evicted;
        QString error;
        if (!context_->addMemory(request.memory, &evicted, &error)) {
            setError(QStringLiteral("写入失败：%1").arg(error));
            return;
        }
        const QString summary =
            evicted.isEmpty()
                ? QStringLiteral("已写入记忆 %1（未入图）").arg(idEdit_->text().trimmed())
                : QStringLiteral("已写入记忆 %1（未入图），短期队列淘汰 %2")
                      .arg(idEdit_->text().trimmed(), evicted);
        setResult(summary);
        emit completed(summary);
        return;
    }

    for (int row = 0; row < entityTable_->rowCount(); ++row) {
        const QTableWidgetItem* nameItem = entityTable_->item(row, kEntityNameColumn);
        const QString name = nameItem == nullptr ? QString() : nameItem->text().trimmed();
        if (name.isEmpty()) continue;
        memory::EntityDraft draft;
        draft.name = name.toStdString();
        auto* typeCombo = qobject_cast<QComboBox*>(entityTable_->cellWidget(row, kEntityTypeColumn));
        if (typeCombo != nullptr) {
            draft.type = static_cast<memory::EntityType>(typeCombo->currentData().toInt());
        }
        const QTableWidgetItem* aliasItem = entityTable_->item(row, kEntityAliasColumn);
        if (aliasItem != nullptr) {
            const QStringList aliasParts = aliasItem->text().split(
                QRegularExpression(QStringLiteral("[,，;；]+")), Qt::SkipEmptyParts);
            for (const auto& alias : aliasParts) draft.aliases.push_back(alias.trimmed().toStdString());
        }
        const QTableWidgetItem* descriptionItem =
            entityTable_->item(row, kEntityDescriptionColumn);
        if (descriptionItem != nullptr) draft.description = descriptionItem->text().toStdString();
        auto* prioritySpin = qobject_cast<QSpinBox*>(
            entityTable_->cellWidget(row, kEntityPriorityColumn));
        if (prioritySpin != nullptr) draft.priority = prioritySpin->value();
        request.entities.push_back(std::move(draft));
    }

    for (int row = 0; row < relationTable_->rowCount(); ++row) {
        memory::RelationDraft draft;
        auto* subjectCombo = qobject_cast<QComboBox*>(
            relationTable_->cellWidget(row, kRelationSubjectColumn));
        auto* objectCombo =
            qobject_cast<QComboBox*>(relationTable_->cellWidget(row, kRelationObjectColumn));
        draft.subject = subjectCombo == nullptr ? QString().toStdString()
                                                : subjectCombo->currentText().trimmed().toStdString();
        draft.object = objectCombo == nullptr ? QString().toStdString()
                                              : objectCombo->currentText().trimmed().toStdString();
        const QTableWidgetItem* relationItem = relationTable_->item(row, kRelationNameColumn);
        draft.relation = relationItem == nullptr ? std::string()
                                                 : relationItem->text().trimmed().toStdString();
        if (draft.subject.empty() || draft.object.empty() || draft.relation.empty()) continue;

        const QTableWidgetItem* descriptionItem =
            relationTable_->item(row, kRelationDescriptionColumn);
        if (descriptionItem != nullptr) draft.description = descriptionItem->text().toStdString();
        auto* sourceCombo =
            qobject_cast<QComboBox*>(relationTable_->cellWidget(row, kRelationSourceColumn));
        if (sourceCombo != nullptr) {
            draft.source = static_cast<memory::EdgeSource>(sourceCombo->currentData().toInt());
        }
        auto* prioritySpin =
            qobject_cast<QSpinBox*>(relationTable_->cellWidget(row, kRelationPriorityColumn));
        if (prioritySpin != nullptr) draft.priority = prioritySpin->value();
        auto* confidenceSpin = qobject_cast<QDoubleSpinBox*>(
            relationTable_->cellWidget(row, kRelationConfidenceColumn));
        if (confidenceSpin != nullptr) draft.confidence = confidenceSpin->value();
        auto* timeEdit = qobject_cast<QDateTimeEdit*>(
            relationTable_->cellWidget(row, kRelationTimeColumn));
        if (timeEdit != nullptr) draft.eventTime = timeEdit->dateTime().toSecsSinceEpoch();
        request.relations.push_back(std::move(draft));
    }

    try {
        const auto result = context_->remember(request);
        const QString summary =
            QStringLiteral("已写入记忆 %1；新建实体 %2 个、合并 %3 个，生成关系 %4 条%5")
                .arg(QString::fromStdString(result.memoryId))
                .arg(result.createdEntityIds.size())
                .arg(result.mergedEntityIds.size())
                .arg(result.edgeIds.size())
                .arg(result.placeholderEntityIds.empty()
                         ? QString()
                         : QStringLiteral("；其中 %1 个为占位实体")
                               .arg(result.placeholderEntityIds.size()));
        setResult(summary);
        emit completed(summary);
    } catch (const std::exception& ex) {
        setError(QStringLiteral("图写入失败，事务已回滚：%1").arg(QString::fromUtf8(ex.what())));
    }
}

void RememberDialog::setError(const QString& text) {
    if (text.isEmpty()) {
        errorLabel_->hide();
        return;
    }
    errorLabel_->setText(text);
    errorLabel_->show();
}

void RememberDialog::setResult(const QString& text) {
    if (text.isEmpty()) {
        resultLabel_->hide();
        return;
    }
    resultLabel_->setText(text);
    resultLabel_->show();
}

}  // namespace gui
