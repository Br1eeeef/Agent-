#pragma once

#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

namespace gui {

// 统一卡片容器：白色圆角面板 + 可选标题，样式由 style.qss 的 [card="true"] 控制。
inline QFrame* makeCard(const QString& title, QWidget* body, QWidget* parent) {
    auto* frame = new QFrame(parent);
    frame->setProperty("card", true);
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);
    if (!title.isEmpty()) {
        auto* header = new QLabel(title, frame);
        header->setProperty("cardTitle", true);
        layout->addWidget(header);
    }
    layout->addWidget(body, 1);
    return frame;
}

}  // namespace gui
