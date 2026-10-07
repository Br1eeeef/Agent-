#pragma once

#include "AppContext.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableView;
class QTextBrowser;

namespace gui {

class MemoryTableModel;

// 记忆管理页：过滤表格 + 详情抽屉 + 增删改与保存。
class MemoryPage : public QWidget {
    Q_OBJECT

public:
    explicit MemoryPage(AppContext* context, QWidget* parent = nullptr);

    void refresh();
    void selectMemory(const QString& id);
    void createMemory();
    void openAdvancedEntry();

signals:
    void recallRequested(const QString& query);
    void toastRequested(const QString& text, bool error);

private:
    QWidget* buildToolbar();
    QWidget* buildTable();
    QWidget* buildDetail();
    void updateDetail();
    void editSelected();
    void removeSelected();
    QString selectedId() const;

    AppContext* context_{nullptr};
    MemoryTableModel* model_{nullptr};
    QTableView* table_{nullptr};

    QLineEdit* searchEdit_{nullptr};
    QComboBox* typeCombo_{nullptr};
    QSpinBox* minImportance_{nullptr};
    QSpinBox* maxImportance_{nullptr};
    QComboBox* sortCombo_{nullptr};
    QLabel* summaryLabel_{nullptr};

    QTextBrowser* detailBrowser_{nullptr};
    QPushButton* recallButton_{nullptr};
    QPushButton* editButton_{nullptr};
    QPushButton* removeButton_{nullptr};
};

}  // namespace gui
