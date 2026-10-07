#include "models/LogTableModel.h"

#include "Format.h"
#include "theme/Theme.h"

namespace gui {

LogTableModel::LogTableModel(AppContext* context, QObject* parent)
    : QAbstractTableModel(parent), context_(context) {}

int LogTableModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(visible_.size());
}

int LogTableModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return ColumnCount;
}

QVariant LogTableModel::data(const QModelIndex& index, int role) const {
    if (context_ == nullptr || !index.isValid() || index.row() < 0 ||
        index.row() >= static_cast<int>(visible_.size())) {
        return {};
    }
    const LogEntry& entry = context_->logger().entries().at(
        visible_[static_cast<std::size_t>(index.row())]);

    switch (role) {
        case Qt::DisplayRole:
            switch (index.column()) {
                case TimeColumn: return entry.time.toString(QStringLiteral("HH:mm:ss"));
                case ActionColumn: return entry.action;
                case TargetColumn: return entry.target;
                case ResultColumn: return entry.ok ? QStringLiteral("成功") : QStringLiteral("失败");
                case ElapsedColumn:
                    return entry.elapsedMs > 0 ? QStringLiteral("%1 ms").arg(entry.elapsedMs)
                                               : QStringLiteral("—");
                case DetailColumn: return entry.detail;
                default: return {};
            }
        case Qt::TextAlignmentRole:
            if (index.column() == TimeColumn || index.column() == ResultColumn ||
                index.column() == ElapsedColumn) {
                return static_cast<int>(Qt::AlignCenter);
            }
            return static_cast<int>(Qt::AlignVCenter | Qt::AlignLeft);
        case Qt::ForegroundRole:
            if (!entry.ok) return QColor(theme::danger());
            if (index.column() == ResultColumn) return QColor(theme::success());
            if (index.column() == ActionColumn) return QColor(theme::accent());
            if (index.column() == DetailColumn) return QColor(theme::textSecondary());
            return {};
        case Qt::BackgroundRole:
            if (!entry.ok) return QColor(theme::alpha(theme::danger(), 0.08));
            return {};
        case Qt::FontRole: {
            QFont font = theme::baseFont(10);
            if (!entry.ok) font.setBold(true);
            return font;
        }
        default:
            return {};
    }
}

QVariant LogTableModel::headerData(int section, Qt::Orientation orientation,
                                   int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    switch (section) {
        case TimeColumn: return QStringLiteral("时间");
        case ActionColumn: return QStringLiteral("操作");
        case TargetColumn: return QStringLiteral("对象");
        case ResultColumn: return QStringLiteral("结果");
        case ElapsedColumn: return QStringLiteral("耗时");
        case DetailColumn: return QStringLiteral("说明");
        default: return {};
    }
}

void LogTableModel::refresh() {
    beginResetModel();
    applyFilter();
    endResetModel();
}

void LogTableModel::setActionFilter(const QString& action) {
    actionFilter_ = action;
    beginResetModel();
    applyFilter();
    endResetModel();
}

void LogTableModel::setOnlyFailures(bool onlyFailures) {
    onlyFailures_ = onlyFailures;
    beginResetModel();
    applyFilter();
    endResetModel();
}

QVector<LogEntry> LogTableModel::visibleEntries() const {
    QVector<LogEntry> result;
    if (context_ == nullptr) return result;
    const auto& entries = context_->logger().entries();
    for (int index : visible_) {
        if (index >= 0 && index < entries.size()) result.append(entries.at(index));
    }
    return result;
}

void LogTableModel::applyFilter() {
    visible_.clear();
    if (context_ == nullptr) return;
    const auto& entries = context_->logger().entries();
    for (int i = 0; i < entries.size(); ++i) {
        const LogEntry& entry = entries.at(i);
        if (onlyFailures_ && entry.ok) continue;
        if (!actionFilter_.isEmpty() && entry.action != actionFilter_) continue;
        visible_.push_back(i);
    }
}

}  // namespace gui
