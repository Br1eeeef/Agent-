#pragma once

#include "AppContext.h"
#include "memory/Memory.h"

#include <QAbstractTableModel>
#include <QString>

#include <vector>

namespace gui {

// 记忆列表模型：只保存ID与记忆快照，刷新时重新向 MemoryManager 取值，不持有核心裸指针。
// 短期队列中残留、但长期存储已删除的ID会以“幽灵行”形式显示。
class MemoryTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column {
        IdColumn = 0,
        ContentColumn,
        TypeColumn,
        ImportanceColumn,
        KeywordsColumn,
        CreatedColumn,
        AccessedColumn,
        ColumnCount,
    };

    enum SortMode { ByCreatedDesc, ByImportanceDesc, ByAccessedDesc, ByIdAsc };

    explicit MemoryTableModel(AppContext* context, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    void refresh();
    void setTextFilter(const QString& text);
    void setTypeFilter(const std::vector<memory::MemoryType>& types);
    void setImportanceRange(int minimum, int maximum);
    void setSortMode(SortMode mode);

    QString idAt(int row) const;
    int rowOfId(const QString& id) const;
    bool isGhost(int row) const;
    int importanceAt(int row) const;
    QString summaryAt(int row) const;
    int visibleCount() const { return static_cast<int>(visible_.size()); }
    int ghostCount() const;

private:
    struct Row {
        QString id;
        memory::Memory snapshot;
        bool ghost{false};
    };

    void applyFilterAndSort();
    bool matches(const Row& row) const;

    AppContext* context_{nullptr};
    std::vector<Row> all_;
    std::vector<int> visible_;
    QString textFilter_;
    std::vector<memory::MemoryType> typeFilter_;
    int minImportance_{1};
    int maxImportance_{5};
    SortMode sortMode_{ByCreatedDesc};
};

}  // namespace gui
