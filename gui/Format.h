#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <vector>

// 界面统一的文本格式化，避免各页面各写一套。
namespace gui::fmt {

inline QString timeText(std::int64_t unixSeconds) {
    if (unixSeconds <= 0) return QStringLiteral("—");
    return QDateTime::fromSecsSinceEpoch(static_cast<qint64>(unixSeconds))
        .toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

// 表格里用短时间，完整时间放在详情面板与提示里。
inline QString shortTimeText(std::int64_t unixSeconds) {
    if (unixSeconds <= 0) return QStringLiteral("—");
    return QDateTime::fromSecsSinceEpoch(static_cast<qint64>(unixSeconds))
        .toString(QStringLiteral("MM-dd HH:mm"));
}

inline QString dateTimeText(const QDateTime& value) {
    if (!value.isValid()) return QStringLiteral("—");
    return value.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

inline QString shorten(const QString& text, int maxChars = 40) {
    QString flat = text;
    flat.replace(QLatin1Char('\n'), QLatin1Char(' '));
    flat = flat.simplified();
    if (flat.size() <= maxChars) return flat;
    return flat.left(maxChars) + QStringLiteral("…");
}

inline QString importanceDots(int level) {
    const int filled = qBound(0, level, 5);
    return QStringLiteral("●").repeated(filled) + QStringLiteral("○").repeated(5 - filled);
}

inline QString joinKeywords(const std::vector<std::string>& keywords, int limit = 3) {
    QStringList parts;
    for (const auto& keyword : keywords) {
        parts << QString::fromStdString(keyword);
        if (limit > 0 && parts.size() >= limit) break;
    }
    return parts.isEmpty() ? QStringLiteral("—") : parts.join(QStringLiteral(" · "));
}

inline QString percent(double ratio) {
    return QStringLiteral("%1%").arg(ratio * 100.0, 0, 'f', 1);
}

inline QString number(double value, int digits = 3) {
    return QString::number(value, 'f', digits);
}

}  // namespace gui::fmt
