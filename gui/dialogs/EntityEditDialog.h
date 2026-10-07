#pragma once

#include "AppContext.h"
#include "memory/Entity.h"

#include <QDialog>
#include <QString>

#include <vector>

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSlider;

namespace gui {

// 实体新增/编辑对话框。编辑模式下由调用方使用 updateEntity 保留创建时间与证据。
class EntityEditDialog : public QDialog {
    Q_OBJECT

public:
    explicit EntityEditDialog(AppContext* context, QWidget* parent = nullptr);

    void loadForEdit(const memory::Entity& entity);
    bool editMode() const { return editMode_; }
    QString editId() const { return editId_; }

    QString entityName() const;
    memory::EntityType entityType() const;
    std::vector<std::string> aliases() const;
    QString description() const;
    int priority() const;

    // 新增模式下由界面生成的实体ID。
    memory::Entity toNewEntity() const;

protected:
    void accept() override;

private:
    void setError(const QString& text);

    AppContext* context_{nullptr};
    bool editMode_{false};
    QString editId_;

    QLineEdit* nameEdit_{nullptr};
    QComboBox* typeCombo_{nullptr};
    QLineEdit* aliasEdit_{nullptr};
    QPlainTextEdit* descriptionEdit_{nullptr};
    QSlider* prioritySlider_{nullptr};
    QLabel* priorityValue_{nullptr};
    QLabel* errorLabel_{nullptr};
    QString newId_;
};

}  // namespace gui
