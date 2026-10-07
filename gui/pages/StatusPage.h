#pragma once

#include "AppContext.h"

#include <QWidget>

#include <vector>

class QCheckBox;
class QComboBox;
class QGridLayout;
class QLabel;
class QPushButton;
class QTableView;

namespace gui {

class KpiCard;
class LogTableModel;

// 系统状态页：容量/负载/文件状态 + 短期队列与 LRU 可视化 + 操作日志 + 危险操作。
class StatusPage : public QWidget {
    Q_OBJECT

public:
    explicit StatusPage(AppContext* context, QWidget* parent = nullptr);

    void refresh();

signals:
    void toastRequested(const QString& text, bool error);

private:
    QWidget* buildCards();
    QWidget* buildQueueCard();
    QWidget* buildLruCard();
    QWidget* buildLogCard();
    QWidget* buildDangerCard();
    void rebuildQueueGrid();
    void rebuildLruList();
    void copySelectedLog();

    AppContext* context_{nullptr};
    KpiCard* queueCard_{nullptr};
    KpiCard* lruCard_{nullptr};
    KpiCard* hashCard_{nullptr};
    KpiCard* fileCard_{nullptr};

    QLabel* queueHeadLabel_{nullptr};
    QWidget* queueGridHost_{nullptr};
    QGridLayout* queueGrid_{nullptr};
    std::vector<QLabel*> queueCells_;

    QWidget* lruListHost_{nullptr};
    QLabel* lruHintLabel_{nullptr};

    LogTableModel* logModel_{nullptr};
    QTableView* logTable_{nullptr};
    QComboBox* logFilter_{nullptr};
    QCheckBox* onlyFailures_{nullptr};
};

}  // namespace gui
