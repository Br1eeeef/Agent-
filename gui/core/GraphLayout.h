#pragma once

#include <QPointF>
#include <QSizeF>

#include <cstddef>
#include <string>
#include <vector>

namespace gui {

// 力导向布局的输入节点：只关心标识与包围盒尺寸，便于脱离界面单独测试。
struct LayoutNode {
    std::string id;
    double width{120.0};
    double height{32.0};
};

struct LayoutEdge {
    std::string from;
    std::string to;
};

// Fruchterman-Reingold 力导向布局。
// 初始位置由节点 id 的哈希决定圆环角度，因此同一份数据每次布局结果完全一致，
// 打开界面时节点不会跳动。结果已裁剪到给定区域内。
class GraphLayout {
public:
    static std::vector<QPointF> compute(const std::vector<LayoutNode>& nodes,
                                        const std::vector<LayoutEdge>& edges,
                                        const QSizeF& area, int iterations = 300);
};

}  // namespace gui
