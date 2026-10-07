#include "models/EntityTableModel.h"

#include "Format.h"
#include "theme/Theme.h"

#include <algorithm>

namespace gui {

EntityTableModel::EntityTableModel(AppContext* context, QObject* parent)
    : QAbstractTableModel(parent), context_(context) {}

int EntityTableModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(visible_.size());
}

int EntityTableModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return ColumnCount;
}

QVariant EntityTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 ||
        index.row() >= static_cast<int>(visible_.size())) {
        return {};
    }
    const memory::Entity& entity =
        all_[static_cast<std::size_t>(visible_[static_cast<std::size_t>(index.row())])];
    const bool active = entity.active;

    switch (role) {
        case Qt::DisplayRole:
            switch (index.column()) {
                case NameColumn: return QString::fromStdString(entity.name);
                case TypeColumn: return theme::entityTypeLabel(entity.type);
                case AliasColumn: {
                    QStringList aliases;
                    for (const auto& alias : entity.aliases) {
                        aliases << QString::fromStdString(alias);
                    }
                    return aliases.isEmpty() ? QStringLiteral("—")
                                             : aliases.join(QStringLiteral("、"));
                }
                case PriorityColumn: return fmt::importanceDots(entity.priority);
                case EdgeCountColumn:
                    return QString::number(edgeCountFor(QString::fromStdString(entity.id)));
                case UpdatedColumn: return fmt::shortTimeText(entity.updatedAt);
                case StatusColumn:
                    return active ? QStringLiteral("活跃") : QStringLiteral("已失效");
                default: return {};
            }
        case Qt::ToolTipRole:
            return QStringLiteral("ID：%1\n描述：%2\n证据记忆：%3")
                .arg(QString::fromStdString(entity.id),
                     entity.description.empty() ? QStringLiteral("—")
                                                : QString::fromStdString(entity.description),
                     QString::number(entity.memoryIds.size()));
        case Qt::TextAlignmentRole:
            if (index.column() == PriorityColumn || index.column() == EdgeCountColumn ||
                index.column() == UpdatedColumn || index.column() == StatusColumn) {
                return static_cast<int>(Qt::AlignCenter);
            }
            return static_cast<int>(Qt::AlignVCenter | Qt::AlignLeft);
        case Qt::ForegroundRole:
            if (!active) return QColor(theme::textMuted());
            if (index.column() == TypeColumn) return theme::entityTypeColor(entity.type);
            if (index.column() == PriorityColumn) return theme::importanceColor(entity.priority);
            if (index.column() == StatusColumn) return QColor(theme::success());
            return {};
        case Qt::BackgroundRole:
            if (!active) return QColor(QStringLiteral("#F2F3F7"));
            if (index.column() == TypeColumn) {
                QColor color = theme::entityTypeColor(entity.type);
                color.setAlphaF(0.12);
                return color;
            }
            return {};
        case Qt::FontRole:
            if (index.column() == NameColumn) {
                QFont font = theme::baseFont(10);
                font.setBold(active);
                font.setStrikeOut(!active);
                return font;
            }
            if (!active) {
                QFont font = theme::baseFont(10);
                font.setStrikeOut(true);
                return font;
            }
            return {};
        default:
            return {};
    }
}

QVariant EntityTableModel::headerData(int section, Qt::Orientation orientation,
                                      int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    switch (section) {
        case NameColumn: return QStringLiteral("实体名称");
        case TypeColumn: return QStringLiteral("类型");
        case AliasColumn: return QStringLiteral("别名");
        case PriorityColumn: return QStringLiteral("优先级");
        case EdgeCountColumn: return QStringLiteral("关联关系");
        case UpdatedColumn: return QStringLiteral("更新时间");
        case StatusColumn: return QStringLiteral("状态");
        default: return {};
    }
}

void EntityTableModel::refresh() {
    beginResetModel();
    all_.clear();
    if (context_ != nullptr) all_ = context_->graph().entities();
    applyFilter();
    endResetModel();
}

void EntityTableModel::setTextFilter(const QString& text) {
    textFilter_ = text.trimmed();
    beginResetModel();
    applyFilter();
    endResetModel();
}

void EntityTableModel::setTypeFilter(const std::vector<memory::EntityType>& types) {
    typeFilter_ = types;
    beginResetModel();
    applyFilter();
    endResetModel();
}

void EntityTableModel::setIncludeInactive(bool include) {
    includeInactive_ = include;
    beginResetModel();
    applyFilter();
    endResetModel();
}

QString EntityTableModel::idAt(int row) const {
    const memory::Entity* entity = entityAt(row);
    return entity == nullptr ? QString() : QString::fromStdString(entity->id);
}

int EntityTableModel::rowOfId(const QString& id) const {
    for (std::size_t i = 0; i < visible_.size(); ++i) {
        if (QString::fromStdString(
                all_[static_cast<std::size_t>(visible_[i])].id) == id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool EntityTableModel::isActive(int row) const {
    const memory::Entity* entity = entityAt(row);
    return entity != nullptr && entity->active;
}

bool EntityTableModel::isPlaceholder(int row) const {
    const memory::Entity* entity = entityAt(row);
    // 占位实体由关系端点自动创建，描述固定为 placeholder。
    return entity != nullptr && entity->description == "placeholder";
}

const memory::Entity* EntityTableModel::entityAt(int row) const {
    if (row < 0 || row >= static_cast<int>(visible_.size())) return nullptr;
    return &all_[static_cast<std::size_t>(visible_[static_cast<std::size_t>(row)])];
}

void EntityTableModel::applyFilter() {
    visible_.clear();
    for (std::size_t i = 0; i < all_.size(); ++i) {
        if (matches(all_[i])) visible_.push_back(static_cast<int>(i));
    }
    std::stable_sort(visible_.begin(), visible_.end(), [this](int left, int right) {
        const memory::Entity& a = all_[static_cast<std::size_t>(left)];
        const memory::Entity& b = all_[static_cast<std::size_t>(right)];
        if (a.active != b.active) return a.active;
        if (a.priority != b.priority) return a.priority > b.priority;
        return a.name < b.name;
    });
}

bool EntityTableModel::matches(const memory::Entity& entity) const {
    if (!includeInactive_ && !entity.active) return false;
    if (!typeFilter_.empty()) {
        if (std::find(typeFilter_.begin(), typeFilter_.end(), entity.type) ==
            typeFilter_.end()) {
            return false;
        }
    }
    if (textFilter_.isEmpty()) return true;
    if (QString::fromStdString(entity.name)
            .contains(textFilter_, Qt::CaseInsensitive)) {
        return true;
    }
    if (QString::fromStdString(entity.description)
            .contains(textFilter_, Qt::CaseInsensitive)) {
        return true;
    }
    for (const auto& alias : entity.aliases) {
        if (QString::fromStdString(alias).contains(textFilter_, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

int EntityTableModel::edgeCountFor(const QString& entityId) const {
    if (context_ == nullptr) return 0;
    int total = 0;
    const std::string id = entityId.toStdString();
    for (const auto& edge : context_->graph().edges()) {
        if (edge.fromEntityId == id || edge.toEntityId == id) ++total;
    }
    return total;
}

}  // namespace gui
