#pragma once

#include "AppContext.h"
#include "memory/Edge.h"

#include <QAbstractTableModel>

#include <vector>

namespace gui {

// 关系列表模型。端点名称在刷新时解析缓存，模型本身不保存核心指针。
class EdgeTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column {
        FromColumn = 0,
        RelationColumn,
        ToColumn,
        SourceColumn,
        ConfidenceColumn,
        PriorityColumn,
        EventTimeColumn,
        EvidenceColumn,
        StatusColumn,
        ColumnCount,
    };

    explicit EdgeTableModel(AppContext* context, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    void refresh();
    void setTextFilter(const QString& text);
    void setIncludeInactive(bool include);
    void setEntityScope(const QString& entityId);  // 只看某个实体的关联边

    QString idAt(int row) const;
    const memory::Edge* edgeAt(int row) const;
    int activeCount() const;

private:
    struct Row {
        memory::Edge edge;
        QString fromName;
        QString toName;
    };

    void applyFilter();
    bool matches(const memory::Edge& edge) const;
    QString nameOf(const std::string& entityId) const;

    AppContext* context_{nullptr};
    std::vector<Row> all_;
    std::vector<int> visible_;
    QString textFilter_;
    QString entityScope_;
    bool includeInactive_{true};
};

}  // namespace gui
