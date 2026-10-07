#pragma once

#include "AppContext.h"
#include "Logger.h"

#include <QAbstractTableModel>

#include <vector>

namespace gui {

// 操作日志模型，供系统状态页展示。
class LogTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column {
        TimeColumn = 0,
        ActionColumn,
        TargetColumn,
        ResultColumn,
        ElapsedColumn,
        DetailColumn,
        ColumnCount,
    };

    explicit LogTableModel(AppContext* context, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    void refresh();
    void setActionFilter(const QString& action);  // 空字符串表示不过滤
    void setOnlyFailures(bool onlyFailures);
    QVector<LogEntry> visibleEntries() const;

private:
    void applyFilter();

    AppContext* context_{nullptr};
    std::vector<int> visible_;
    QString actionFilter_;
    bool onlyFailures_{false};
};

}  // namespace gui
