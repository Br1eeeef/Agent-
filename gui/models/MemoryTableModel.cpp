#include "models/MemoryTableModel.h"

#include "Format.h"
#include "theme/Theme.h"

#include <algorithm>
#include <unordered_set>

namespace gui {

MemoryTableModel::MemoryTableModel(AppContext* context, QObject* parent)
    : QAbstractTableModel(parent), context_(context) {}

int MemoryTableModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(visible_.size());
}

int MemoryTableModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return ColumnCount;
}

QVariant MemoryTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 ||
        index.row() >= static_cast<int>(visible_.size())) {
        return {};
    }
    const Row& row = all_[static_cast<std::size_t>(visible_[static_cast<std::size_t>(index.row())])];
    const memory::Memory& value = row.snapshot;

    switch (role) {
        case Qt::DisplayRole:
            switch (index.column()) {
                case IdColumn: return row.id;
                case ContentColumn:
                    return row.ghost ? QStringLiteral("（长期记忆中已删除，仅短期队列残留）")
                                     : fmt::shorten(QString::fromStdString(value.content), 28);
                case TypeColumn: return theme::memoryTypeLabel(value.type);
                case ImportanceColumn: return fmt::importanceDots(value.importance);
                case KeywordsColumn:
                    return fmt::joinKeywords(value.keywords, 2);
                case CreatedColumn: return fmt::shortTimeText(value.createdAt);
                case AccessedColumn: return fmt::shortTimeText(value.lastAccessedAt);
                default: return {};
            }
        case Qt::ToolTipRole:
            return QStringLiteral("ID：%1\n创建：%2\n最近访问：%3\n\n%4")
                .arg(row.id, fmt::timeText(value.createdAt), fmt::timeText(value.lastAccessedAt),
                     QString::fromStdString(value.content));
        case Qt::TextAlignmentRole:
            if (index.column() == ImportanceColumn || index.column() == CreatedColumn ||
                index.column() == AccessedColumn) {
                return static_cast<int>(Qt::AlignCenter);
            }
            return static_cast<int>(Qt::AlignVCenter | Qt::AlignLeft);
        case Qt::ForegroundRole:
            if (row.ghost) return QColor(theme::textMuted());
            if (index.column() == TypeColumn) return theme::memoryTypeColor(value.type);
            if (index.column() == ImportanceColumn) {
                return theme::importanceColor(value.importance);
            }
            if (index.column() == KeywordsColumn) return QColor(theme::textSecondary());
            return {};
        case Qt::BackgroundRole:
            if (row.ghost) return QColor(QStringLiteral("#F2F3F7"));
            if (index.column() == TypeColumn) {
                QColor color = theme::memoryTypeColor(value.type);
                color.setAlphaF(0.12);
                return color;
            }
            return {};
        case Qt::FontRole:
            if (row.ghost) {
                QFont font = theme::baseFont(10);
                font.setStrikeOut(true);
                return font;
            }
            if (index.column() == IdColumn) {
                QFont font = theme::baseFont(10);
                font.setBold(true);
                return font;
            }
            return {};
        default:
            return {};
    }
}

QVariant MemoryTableModel::headerData(int section, Qt::Orientation orientation,
                                     int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    switch (section) {
        case IdColumn: return QStringLiteral("ID");
        case ContentColumn: return QStringLiteral("内容摘要");
        case TypeColumn: return QStringLiteral("类型");
        case ImportanceColumn: return QStringLiteral("重要度");
        case KeywordsColumn: return QStringLiteral("关键词");
        case CreatedColumn: return QStringLiteral("创建时间");
        case AccessedColumn: return QStringLiteral("最近访问");
        default: return {};
    }
}

void MemoryTableModel::refresh() {
    beginResetModel();
    all_.clear();
    if (context_ != nullptr) {
        for (const auto& value : context_->manager().all()) {
            Row row;
            row.id = QString::fromStdString(value.id);
            row.snapshot = value;
            row.ghost = false;
            all_.push_back(std::move(row));
        }
        std::unordered_set<std::string> known;
        for (const auto& row : all_) known.insert(row.id.toStdString());
        for (const auto& id : context_->manager().recentIds()) {
            if (known.count(id) != 0) continue;
            Row row;
            row.id = QString::fromStdString(id);
            row.snapshot.id = id;
            row.snapshot.content = QStringLiteral("（已删除）").toStdString();
            row.ghost = true;
            all_.push_back(std::move(row));
        }
    }
    applyFilterAndSort();
    endResetModel();
}

void MemoryTableModel::setTextFilter(const QString& text) {
    textFilter_ = text.trimmed();
    beginResetModel();
    applyFilterAndSort();
    endResetModel();
}

void MemoryTableModel::setTypeFilter(const std::vector<memory::MemoryType>& types) {
    typeFilter_ = types;
    beginResetModel();
    applyFilterAndSort();
    endResetModel();
}

void MemoryTableModel::setImportanceRange(int minimum, int maximum) {
    minImportance_ = qBound(1, std::min(minimum, maximum), 5);
    maxImportance_ = qBound(1, std::max(minimum, maximum), 5);
    beginResetModel();
    applyFilterAndSort();
    endResetModel();
}

void MemoryTableModel::setSortMode(SortMode mode) {
    sortMode_ = mode;
    beginResetModel();
    applyFilterAndSort();
    endResetModel();
}

QString MemoryTableModel::idAt(int row) const {
    if (row < 0 || row >= static_cast<int>(visible_.size())) return {};
    return all_[static_cast<std::size_t>(visible_[static_cast<std::size_t>(row)])].id;
}

int MemoryTableModel::rowOfId(const QString& id) const {
    for (std::size_t i = 0; i < visible_.size(); ++i) {
        if (all_[static_cast<std::size_t>(visible_[i])].id == id) return static_cast<int>(i);
    }
    return -1;
}

bool MemoryTableModel::isGhost(int row) const {
    if (row < 0 || row >= static_cast<int>(visible_.size())) return false;
    return all_[static_cast<std::size_t>(visible_[static_cast<std::size_t>(row)])].ghost;
}

int MemoryTableModel::importanceAt(int row) const {
    if (row < 0 || row >= static_cast<int>(visible_.size())) return 0;
    return all_[static_cast<std::size_t>(visible_[static_cast<std::size_t>(row)])]
        .snapshot.importance;
}

QString MemoryTableModel::summaryAt(int row) const {
    if (row < 0 || row >= static_cast<int>(visible_.size())) return {};
    return QString::fromStdString(
        all_[static_cast<std::size_t>(visible_[static_cast<std::size_t>(row)])].snapshot.content);
}

int MemoryTableModel::ghostCount() const {
    int total = 0;
    for (std::size_t i = 0; i < visible_.size(); ++i) {
        if (all_[static_cast<std::size_t>(visible_[i])].ghost) ++total;
    }
    return total;
}

bool MemoryTableModel::matches(const Row& row) const {
    if (row.ghost && !textFilter_.isEmpty()) {
        return row.id.contains(textFilter_, Qt::CaseInsensitive);
    }
    if (!textFilter_.isEmpty()) {
        const QString content = QString::fromStdString(row.snapshot.content);
        bool hit = row.id.contains(textFilter_, Qt::CaseInsensitive) ||
                   content.contains(textFilter_, Qt::CaseInsensitive);
        if (!hit) {
            for (const auto& keyword : row.snapshot.keywords) {
                if (QString::fromStdString(keyword).contains(textFilter_, Qt::CaseInsensitive)) {
                    hit = true;
                    break;
                }
            }
        }
        if (!hit) return false;
    }
    if (!typeFilter_.empty() && !row.ghost) {
        if (std::find(typeFilter_.begin(), typeFilter_.end(), row.snapshot.type) ==
            typeFilter_.end()) {
            return false;
        }
    }
    if (!row.ghost) {
        if (row.snapshot.importance < minImportance_ ||
            row.snapshot.importance > maxImportance_) {
            return false;
        }
    }
    return true;
}

void MemoryTableModel::applyFilterAndSort() {
    visible_.clear();
    for (std::size_t i = 0; i < all_.size(); ++i) {
        if (matches(all_[i])) visible_.push_back(static_cast<int>(i));
    }
    std::stable_sort(visible_.begin(), visible_.end(), [this](int left, int right) {
        const Row& a = all_[static_cast<std::size_t>(left)];
        const Row& b = all_[static_cast<std::size_t>(right)];
        // 幽灵行永远排在最后，避免与真实数据混在一起。
        if (a.ghost != b.ghost) return !a.ghost;
        switch (sortMode_) {
            case ByImportanceDesc:
                if (a.snapshot.importance != b.snapshot.importance) {
                    return a.snapshot.importance > b.snapshot.importance;
                }
                return a.snapshot.createdAt > b.snapshot.createdAt;
            case ByAccessedDesc:
                if (a.snapshot.lastAccessedAt != b.snapshot.lastAccessedAt) {
                    return a.snapshot.lastAccessedAt > b.snapshot.lastAccessedAt;
                }
                return a.snapshot.createdAt > b.snapshot.createdAt;
            case ByIdAsc:
                return a.id < b.id;
            case ByCreatedDesc:
            default:
                if (a.snapshot.createdAt != b.snapshot.createdAt) {
                    return a.snapshot.createdAt > b.snapshot.createdAt;
                }
                return a.id < b.id;
        }
    });
}

}  // namespace gui
