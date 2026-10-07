#pragma once

#include "AppContext.h"
#include "memory/Entity.h"

#include <QAbstractTableModel>

#include <vector>

namespace gui {

// 实体列表模型。行数据是 Entity 的值拷贝，刷新时重建，不跨刷新持有核心指针。
class EntityTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column {
        NameColumn = 0,
        TypeColumn,
        AliasColumn,
        PriorityColumn,
        EdgeCountColumn,
        UpdatedColumn,
        StatusColumn,
        ColumnCount,
    };

    explicit EntityTableModel(AppContext* context, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    void refresh();
    void setTextFilter(const QString& text);
    void setTypeFilter(const std::vector<memory::EntityType>& types);
    void setIncludeInactive(bool include);

    QString idAt(int row) const;
    int rowOfId(const QString& id) const;
    bool isActive(int row) const;
    bool isPlaceholder(int row) const;
    const memory::Entity* entityAt(int row) const;

private:
    void applyFilter();
    bool matches(const memory::Entity& entity) const;
    int edgeCountFor(const QString& entityId) const;

    AppContext* context_{nullptr};
    std::vector<memory::Entity> all_;
    std::vector<int> visible_;
    QString textFilter_;
    std::vector<memory::EntityType> typeFilter_;
    bool includeInactive_{true};
};

}  // namespace gui
