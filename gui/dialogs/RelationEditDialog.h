#pragma once

#include "AppContext.h"
#include "memory/Edge.h"

#include <QDialog>
#include <QString>
#include <QStringList>

class QCheckBox;
class QComboBox;
class QDateTimeEdit;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QSlider;
class QSpinBox;

namespace gui {

// 关系新增/修改对话框。端点不存在时会创建占位实体，界面明确提示这一点。
class RelationEditDialog : public QDialog {
    Q_OBJECT

public:
    explicit RelationEditDialog(AppContext* context, QWidget* parent = nullptr);

    struct Result {
        QString subject;
        QString object;
        QString relation;
        QString description;
        memory::EdgeSource source{memory::EdgeSource::User};
        int priority{3};
        double confidence{1.0};
        std::int64_t eventTime{0};
    };

    void prefill(const QString& subject, const QString& object);
    Result result() const;

signals:
    void submitted();

protected:
    void accept() override;

private:
    void addEndpointItems();
    void setError(const QString& text);

    AppContext* context_{nullptr};
    QComboBox* subjectCombo_{nullptr};
    QComboBox* objectCombo_{nullptr};
    QLineEdit* relationEdit_{nullptr};
    QLineEdit* descriptionEdit_{nullptr};
    QComboBox* sourceCombo_{nullptr};
    QSlider* prioritySlider_{nullptr};
    QLabel* priorityValue_{nullptr};
    QDoubleSpinBox* confidenceSpin_{nullptr};
    QCheckBox* useNowCheck_{nullptr};
    QDateTimeEdit* eventTimeEdit_{nullptr};
    QLabel* hintLabel_{nullptr};
    QLabel* errorLabel_{nullptr};
};

}  // namespace gui
