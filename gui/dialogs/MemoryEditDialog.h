#pragma once

#include "AppContext.h"
#include "memory/Memory.h"

#include <QDialog>
#include <QString>

#include <vector>

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSlider;

namespace gui {

// 记忆新增/编辑对话框：提交前做非空、重复、重要度校验，异常以内联错误条呈现。
class MemoryEditDialog : public QDialog {
    Q_OBJECT

public:
    explicit MemoryEditDialog(AppContext* context, QWidget* parent = nullptr);

    // 进入编辑模式：ID 锁定为原值。
    void loadForEdit(const memory::Memory& value);
    bool editMode() const { return editMode_; }

    memory::Memory toMemory() const;
    std::vector<std::string> keywords() const;
    QString id() const;

protected:
    void accept() override;

private:
    QString suggestId() const;
    void setError(const QString& text);

    AppContext* context_{nullptr};
    bool editMode_{false};

    QLineEdit* idEdit_{nullptr};
    QPlainTextEdit* contentEdit_{nullptr};
    QComboBox* typeCombo_{nullptr};
    QSlider* importanceSlider_{nullptr};
    QLabel* importanceValue_{nullptr};
    QLineEdit* keywordsEdit_{nullptr};
    QLabel* errorLabel_{nullptr};
};

}  // namespace gui
