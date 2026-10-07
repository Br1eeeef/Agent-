#pragma once

#include "memory/Edge.h"
#include "memory/Entity.h"
#include "memory/Memory.h"

#include <QColor>
#include <QFont>
#include <QString>

// 浅色现代卡片风的统一配色与语义色，界面各处只能从这里取色。
namespace gui::theme {

inline QString canvas() { return QStringLiteral("#F5F6FA"); }
inline QString surface() { return QStringLiteral("#FFFFFF"); }
inline QString border() { return QStringLiteral("#E6E8F0"); }
inline QString accent() { return QStringLiteral("#4F6BED"); }
inline QString success() { return QStringLiteral("#22B8A6"); }
inline QString danger() { return QStringLiteral("#EB5757"); }
inline QString warning() { return QStringLiteral("#F2994A"); }

inline QString textPrimary() { return QStringLiteral("#1F2430"); }
inline QString textSecondary() { return QStringLiteral("#6B7280"); }
inline QString textMuted() { return QStringLiteral("#9AA3B2"); }
inline QString separator() { return QStringLiteral("#EEF0F5"); }

inline QString rowZebra() { return QStringLiteral("#FAFBFD"); }
inline QString rowHover() { return QStringLiteral("#EEF2FE"); }
inline QString rowSelected() { return QStringLiteral("#E3EAFE"); }

// 供 QSS 拼接使用：把 #RRGGBB 转成带透明度的 rgba()。
inline QString alpha(const QString& hex, double opacity) {
    const QColor color(hex);
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(opacity, 0, 'f', 3);
}

inline QColor memoryTypeColor(memory::MemoryType type) {
    switch (type) {
        case memory::MemoryType::Profile: return QColor("#7C5CFC");
        case memory::MemoryType::Plan: return QColor("#2F80ED");
        case memory::MemoryType::Conversation: return QColor("#F2994A");
        case memory::MemoryType::Event: return QColor("#EB5757");
        default: return QColor("#828282");
    }
}

inline QColor entityTypeColor(memory::EntityType type) {
    switch (type) {
        case memory::EntityType::Concept: return QColor("#2F80ED");
        case memory::EntityType::Fact: return QColor("#22B8A6");
        case memory::EntityType::Preference: return QColor("#7C5CFC");
        default: return QColor("#828282");
    }
}

inline QColor importanceColor(int level) {
    switch (level) {
        case 1: return QColor("#A8B4C4");
        case 2: return QColor("#59C3A8");
        case 3: return QColor("#4F6BED");
        case 4: return QColor("#F2994A");
        default: return QColor("#E4572E");
    }
}

inline QColor scoreColor(double value) {
    // 得分条：低分偏灰蓝，高分偏主色，避免使用红绿造成误读。
    const double clamped = qBound(0.0, value, 1.0);
    QColor low("#A8B4C4");
    QColor high("#4F6BED");
    return QColor::fromRgbF(
        low.redF() + (high.redF() - low.redF()) * clamped,
        low.greenF() + (high.greenF() - low.greenF()) * clamped,
        low.blueF() + (high.blueF() - low.blueF()) * clamped);
}

inline QString memoryTypeLabel(memory::MemoryType type) {
    switch (type) {
        case memory::MemoryType::Profile: return QStringLiteral("画像");
        case memory::MemoryType::Plan: return QStringLiteral("计划");
        case memory::MemoryType::Conversation: return QStringLiteral("对话");
        case memory::MemoryType::Event: return QStringLiteral("事件");
        default: return QStringLiteral("其他");
    }
}

inline QString entityTypeLabel(memory::EntityType type) {
    switch (type) {
        case memory::EntityType::Concept: return QStringLiteral("概念");
        case memory::EntityType::Fact: return QStringLiteral("事实");
        case memory::EntityType::Preference: return QStringLiteral("偏好");
        default: return QStringLiteral("其他");
    }
}

inline QString edgeSourceLabel(memory::EdgeSource source) {
    switch (source) {
        case memory::EdgeSource::User: return QStringLiteral("用户");
        case memory::EdgeSource::Agent: return QStringLiteral("代理");
        case memory::EdgeSource::Extractor: return QStringLiteral("抽取");
        default: return QStringLiteral("共现");
    }
}

// 界面基础字体：中文优先雅黑，回退 Segoe UI。
inline QFont baseFont(int pointSize = 10, bool bold = false) {
    QFont font(QStringLiteral("Microsoft YaHei UI"), pointSize);
    font.setStyleHint(QFont::SansSerif);
    font.setBold(bold);
    return font;
}

}  // namespace gui::theme
