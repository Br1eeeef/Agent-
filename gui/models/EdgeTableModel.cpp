#include "models/EdgeTableModel.h"

#include "Format.h"
#include "theme/Theme.h"

namespace gui {

EdgeTableModel::EdgeTableModel(AppContext* context, QObject* parent)
    : QAbstractTableModel(parent), context_(context) {}

int EdgeTableModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(visible_.size());
}

int EdgeTableModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return ColumnCount;
}

QVariant EdgeTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 ||
        index.row() >= static_cast<int>(visible_.size())) {
        return {};
    }
    const Row& row =
        all_[static_cast<std::size_t>(visible_[static_cast<std::size_t>(index.row())])];
    const memory::Edge& edge = row.edge;

    switch (role) {
        case Qt::DisplayRole:
            switch (index.column()) {
                case FromColumn: return row.fromName;
                case RelationColumn: return QString::fromStdString(edge.relation);
                case ToColumn: return row.toName;
                case SourceColumn: return theme::edgeSourceLabel(edge.source);
                case ConfidenceColumn: return fmt::number(edge.confidence, 2);
                case PriorityColumn: return fmt::importanceDots(edge.priority);
                case EventTimeColumn: return fmt::shortTimeText(edge.eventTime);
                case EvidenceColumn: return QString::number(edge.memoryIds.size());
                case StatusColumn:
                    return edge.active ? QStringLiteral("活跃") : QStringLiteral("已失效");
                default: return {};
            }
        case Qt::ToolTipRole:
            return QStringLiteral("边ID：%1\n描述：%2\n证据记忆：%3\n失效原因：%4")
                .arg(QString::fromStdString(edge.id),
                     edge.description.empty() ? QStringLiteral("—")
                                              : QString::fromStdString(edge.description),
                     edge.memoryIds.empty()
                         ? QStringLiteral("—")
                         : QString::fromStdString(edge.memoryIds.front()),
                     edge.invalidReason.empty()
                         ? QStringLiteral("—")
                         : QString::fromStdString(edge.invalidReason));
        case Qt::TextAlignmentRole:
            if (index.column() != FromColumn && index.column() != ToColumn &&
                index.column() != RelationColumn) {
                return static_cast<int>(Qt::AlignCenter);
            }
            return static_cast<int>(Qt::AlignVCenter | Qt::AlignLeft);
        case Qt::ForegroundRole:
            if (!edge.active) return QColor(theme::textMuted());
            if (index.column() == RelationColumn) return QColor(theme::accent());
            if (index.column() == StatusColumn) return QColor(theme::success());
            return {};
        case Qt::BackgroundRole:
            if (!edge.active) return QColor(QStringLiteral("#F2F3F7"));
            return {};
        case Qt::FontRole: {
            QFont font = theme::baseFont(10);
            if (!edge.active) font.setStrikeOut(true);
            if (index.column() == RelationColumn) font.setBold(true);
            return font;
        }
        default:
            return {};
    }
}

QVariant EdgeTableModel::headerData(int section, Qt::Orientation orientation,
                                    int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    switch (section) {
        case FromColumn: return QStringLiteral("起点");
        case RelationColumn: return QStringLiteral("关系");
        case ToColumn: return QStringLiteral("终点");
        case SourceColumn: return QStringLiteral("来源");
        case ConfidenceColumn: return QStringLiteral("置信度");
        case PriorityColumn: return QStringLiteral("优先级");
        case EventTimeColumn: return QStringLiteral("事件时间");
        case EvidenceColumn: return QStringLiteral("证据数");
        case StatusColumn: return QStringLiteral("状态");
        default: return {};
    }
}

void EdgeTableModel::refresh() {
    beginResetModel();
    all_.clear();
    if (context_ != nullptr) {
        for (const auto& edge : context_->graph().edges()) {
            Row row;
            row.fromName = nameOf(edge.fromEntityId);
            row.toName = nameOf(edge.toEntityId);
            row.edge = edge;
            all_.push_back(std::move(row));
        }
    }
    applyFilter();
    endResetModel();
}

void EdgeTableModel::setTextFilter(const QString& text) {
    textFilter_ = text.trimmed();
    beginResetModel();
    applyFilter();
    endResetModel();
}

void EdgeTableModel::setIncludeInactive(bool include) {
    includeInactive_ = include;
    beginResetModel();
    applyFilter();
    endResetModel();
}

void EdgeTableModel::setEntityScope(const QString& entityId) {
    entityScope_ = entityId;
    beginResetModel();
    applyFilter();
    endResetModel();
}

QString EdgeTableModel::idAt(int row) const {
    const memory::Edge* edge = edgeAt(row);
    return edge == nullptr ? QString() : QString::fromStdString(edge->id);
}

const memory::Edge* EdgeTableModel::edgeAt(int row) const {
    if (row < 0 || row >= static_cast<int>(visible_.size())) return nullptr;
    return &all_[static_cast<std::size_t>(visible_[static_cast<std::size_t>(row)])].edge;
}

int EdgeTableModel::activeCount() const {
    int total = 0;
    for (const auto& row : all_) {
        if (row.edge.active) ++total;
    }
    return total;
}

void EdgeTableModel::applyFilter() {
    visible_.clear();
    for (std::size_t i = 0; i < all_.size(); ++i) {
        if (matches(all_[i].edge)) visible_.push_back(static_cast<int>(i));
    }
    std::stable_sort(visible_.begin(), visible_.end(), [this](int left, int right) {
        const Row& a = all_[static_cast<std::size_t>(left)];
        const Row& b = all_[static_cast<std::size_t>(right)];
        if (a.edge.active != b.edge.active) return a.edge.active;
        if (a.edge.priority != b.edge.priority) return a.edge.priority > b.edge.priority;
        return a.edge.id < b.edge.id;
    });
}

bool EdgeTableModel::matches(const memory::Edge& edge) const {
    if (!includeInactive_ && !edge.active) return false;
    if (!entityScope_.isEmpty() && edge.fromEntityId != entityScope_.toStdString() &&
        edge.toEntityId != entityScope_.toStdString()) {
        return false;
    }
    if (textFilter_.isEmpty()) return true;
    return QString::fromStdString(edge.relation)
               .contains(textFilter_, Qt::CaseInsensitive) ||
           QString::fromStdString(edge.id).contains(textFilter_, Qt::CaseInsensitive) ||
           nameOf(edge.fromEntityId).contains(textFilter_, Qt::CaseInsensitive) ||
           nameOf(edge.toEntityId).contains(textFilter_, Qt::CaseInsensitive);
}

QString EdgeTableModel::nameOf(const std::string& entityId) const {
    if (context_ == nullptr) return QString::fromStdString(entityId);
    const memory::Entity* entity = context_->graph().findEntity(entityId);
    if (entity == nullptr) return QStringLiteral("%1（已删除）").arg(QString::fromStdString(entityId));
    return QString::fromStdString(entity->name);
}

}  // namespace gui
