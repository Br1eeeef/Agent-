#pragma once

#include "AppContext.h"

#include <QDialog>
#include <QString>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSlider;
class QTableWidget;

namespace gui {

// 高级录入：一次写入“记忆 + 实体草稿 + 关系草稿”，调用 GraphMemoryService::remember。
// 实体与关系全部手工填写，不接大模型；“从内容提候选”仅为本地启发式辅助。
class RememberDialog : public QDialog {
    Q_OBJECT

public:
    explicit RememberDialog(AppContext* context, QWidget* parent = nullptr);

    void prefillContent(const QString& content);

signals:
    void completed(const QString& summary);

private:
    void buildMemoryTab();
    void buildEntityTab();
    void buildRelationTab();
    void addEntityRow(const QString& name = QString(), int typeIndex = 0,
                      const QString& aliases = QString(), const QString& description = QString(),
                      int priority = 3);
    void addRelationRow(const QString& subject = QString(), const QString& object = QString(),
                        const QString& relation = QString());
    void removeSelectedRow(QTableWidget* table);
    void suggestCandidates();
    void submit();
    void refreshEndpointChoices();
    void setError(const QString& text);
    void setResult(const QString& text);
    QStringList entityNames() const;
    std::vector<std::string> parseKeywords(const QString& text) const;

    AppContext* context_{nullptr};
    QPlainTextEdit* contentEdit_{nullptr};
    QLineEdit* idEdit_{nullptr};
    QComboBox* typeCombo_{nullptr};
    QSlider* importanceSlider_{nullptr};
    QLabel* importanceValue_{nullptr};
    QLineEdit* keywordsEdit_{nullptr};
    QTableWidget* entityTable_{nullptr};
    QTableWidget* relationTable_{nullptr};
    QCheckBox* memoryOnlyCheck_{nullptr};
    QLabel* errorLabel_{nullptr};
    QLabel* resultLabel_{nullptr};
};

}  // namespace gui
