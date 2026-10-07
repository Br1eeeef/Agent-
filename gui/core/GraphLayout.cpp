#include "GraphLayout.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace gui {
namespace {

constexpr double kTwoPi = 6.283185307179586;

// 稳定哈希：不依赖标准库实现，保证同一 id 在不同编译器下得到同样的种子角度。
std::uint64_t stableHash(const std::string& text) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (unsigned char ch : text) {
        hash ^= ch;
        hash *= 1099511628211ULL;
    }
    return hash;
}

double normalizedAngle(const std::string& id) {
    const std::uint64_t hash = stableHash(id);
    return static_cast<double>(hash % 1000000ULL) / 1000000.0 * kTwoPi;
}

}  // namespace

std::vector<QPointF> GraphLayout::compute(const std::vector<LayoutNode>& nodes,
                                          const std::vector<LayoutEdge>& edges,
                                          const QSizeF& area, int iterations) {
    std::vector<QPointF> result;
    if (nodes.empty()) return result;

    const double width = std::max(area.width(), 320.0);
    const double height = std::max(area.height(), 240.0);
    const double margin = 56.0;
    const double innerWidth = std::max(width - margin * 2.0, 80.0);
    const double innerHeight = std::max(height - margin * 2.0, 80.0);
    const double centerX = width / 2.0;
    const double centerY = height / 2.0;

    const std::size_t count = nodes.size();
    result.resize(count);

    // 1. 确定性初始位置：id 哈希决定角度，半径按序号轻微错开，避免完全重叠。
    const double radiusX = innerWidth * 0.36;
    const double radiusY = innerHeight * 0.36;
    for (std::size_t i = 0; i < count; ++i) {
        const double angle = normalizedAngle(nodes[i].id);
        const double ring = 0.72 + 0.28 * static_cast<double>((i % 3)) / 2.0;
        result[i] = QPointF(centerX + std::cos(angle) * radiusX * ring,
                           centerY + std::sin(angle) * radiusY * ring);
    }
    if (count == 1) {
        result[0] = QPointF(centerX, centerY);
        return result;
    }

    std::unordered_map<std::string, std::size_t> index;
    index.reserve(count * 2);
    for (std::size_t i = 0; i < count; ++i) index[nodes[i].id] = i;

    // 2. 力导向迭代：斥力 + 边拉力。
    // 节点是带文字的矩形，理想间距必须参考节点尺寸，否则按面积算出的 k 太小，
    // 矩形会互相压住，边上写的关系文字也就被遮住。
    double averageWidth = 0.0;
    double averageHeight = 0.0;
    for (const auto& node : nodes) {
        averageWidth += node.width;
        averageHeight += node.height;
    }
    averageWidth /= static_cast<double>(count);
    averageHeight /= static_cast<double>(count);

    const double areaK = std::sqrt(innerWidth * innerHeight / static_cast<double>(count)) * 0.9;
    const double k = std::max(areaK, averageWidth * 1.15 + 30.0);
    const double initialTemperature = std::min(innerWidth, innerHeight) * 0.12;
    std::vector<QPointF> displacement(count);

    for (int step = 0; step < iterations; ++step) {
        const double t = initialTemperature *
                         (1.0 - static_cast<double>(step) / static_cast<double>(iterations));
        std::fill(displacement.begin(), displacement.end(), QPointF(0.0, 0.0));

        // 斥力：所有节点两两排斥。
        for (std::size_t i = 0; i < count; ++i) {
            for (std::size_t j = i + 1; j < count; ++j) {
                QPointF delta = result[i] - result[j];
                double distance = std::hypot(delta.x(), delta.y());
                if (distance < 0.01) {
                    // 完全重合时用确定性偏移拆开，避免除以零产生 NaN。
                    delta = QPointF(((i + j) % 2 == 0) ? 0.5 : -0.5,
                                    ((i + j) % 3 == 0) ? 0.5 : -0.5);
                    distance = std::hypot(delta.x(), delta.y());
                }
                const double force = (k * k) / distance;
                const QPointF unit = delta / distance;
                displacement[i] += unit * force;
                displacement[j] -= unit * force;
            }
        }

        // 引力：有边相连的节点互相靠拢。
        for (const auto& edge : edges) {
            const auto fromIt = index.find(edge.from);
            const auto toIt = index.find(edge.to);
            if (fromIt == index.end() || toIt == index.end()) continue;
            const std::size_t i = fromIt->second;
            const std::size_t j = toIt->second;
            if (i == j) continue;
            QPointF delta = result[i] - result[j];
            double distance = std::hypot(delta.x(), delta.y());
            if (distance < 0.01) distance = 0.01;
            const double force = (distance * distance) / k;
            const QPointF unit = delta / distance;
            displacement[i] -= unit * force;
            displacement[j] += unit * force;
        }

        // 位移与裁剪：始终把节点限制在可视区域内，避免跑出画布。
        for (std::size_t i = 0; i < count; ++i) {
            const double length = std::hypot(displacement[i].x(), displacement[i].y());
            if (length > t && length > 0.0) {
                displacement[i] = displacement[i] / length * t;
            }
            result[i] += displacement[i];
            result[i].setX(std::clamp(result[i].x(), margin, width - margin));
            result[i].setY(std::clamp(result[i].y(), margin, height - margin));
        }
    }

    // 3. 去重叠松弛：力导向不保证矩形互不覆盖，这里按矩形相交做若干轮分离，
    //    让每个节点的完整标签与它周边的关系文字都能露出来。
    //    gapX/gapY 是节点之间要保留的呼吸空间。
    const double gapX = 46.0;
    const double gapY = 30.0;
    constexpr int kRelaxPasses = 160;
    for (int pass = 0; pass < kRelaxPasses; ++pass) {
        bool moved = false;
        for (std::size_t i = 0; i < count; ++i) {
            for (std::size_t j = i + 1; j < count; ++j) {
                const double minDx = (nodes[i].width + nodes[j].width) / 2.0 + gapX;
                const double minDy = (nodes[i].height + nodes[j].height) / 2.0 + gapY;
                double dx = result[j].x() - result[i].x();
                double dy = result[j].y() - result[i].y();
                if (std::abs(dx) >= minDx || std::abs(dy) >= minDy) continue;

                // 沿穿透较浅的轴分开；完全重合时用节点序号决定方向，保证结果确定。
                const double pushX = minDx - std::abs(dx);
                const double pushY = minDy - std::abs(dy);
                if (pushX <= pushY) {
                    const double sign = dx != 0.0 ? (dx > 0.0 ? 1.0 : -1.0)
                                                  : ((i + j) % 2 == 0 ? 1.0 : -1.0);
                    const double step = pushX / 2.0 + 0.5;
                    result[i].setX(result[i].x() - sign * step);
                    result[j].setX(result[j].x() + sign * step);
                } else {
                    const double sign = dy != 0.0 ? (dy > 0.0 ? 1.0 : -1.0)
                                                  : ((i + j) % 2 == 0 ? 1.0 : -1.0);
                    const double step = pushY / 2.0 + 0.5;
                    result[i].setY(result[i].y() - sign * step);
                    result[j].setY(result[j].y() + sign * step);
                }
                moved = true;
            }
        }
        // 每轮结束都裁剪回画布，避免节点被推出可视区域。
        for (std::size_t i = 0; i < count; ++i) {
            result[i].setX(std::clamp(result[i].x(), margin, width - margin));
            result[i].setY(std::clamp(result[i].y(), margin, height - margin));
        }
        if (!moved) break;
    }

    // 4. 让节点避开连线：节点互相分开之后，仍可能有连线恰好从第三个节点身上穿过，
    //    那条线会被节点挡住、看起来断成两截。这里把挡路的节点沿法线推出去。
    constexpr int kEdgeReliefPasses = 40;
    for (int pass = 0; pass < kEdgeReliefPasses; ++pass) {
        bool moved = false;
        for (const auto& edge : edges) {
            const auto fromIt = index.find(edge.from);
            const auto toIt = index.find(edge.to);
            if (fromIt == index.end() || toIt == index.end()) continue;
            const std::size_t a = fromIt->second;
            const std::size_t b = toIt->second;
            if (a == b) continue;

            const QPointF direction = result[b] - result[a];
            const double length = std::hypot(direction.x(), direction.y());
            if (length < 1.0) continue;
            const QPointF unit = direction / length;

            for (std::size_t w = 0; w < count; ++w) {
                if (w == a || w == b) continue;
                const QPointF relative = result[w] - result[a];
                const double t =
                    std::clamp(relative.x() * unit.x() + relative.y() * unit.y(), 0.0, length);
                const QPointF closest = result[a] + unit * t;
                const QPointF delta = result[w] - closest;
                const double radiusX = nodes[w].width / 2.0 + 18.0;
                const double radiusY = nodes[w].height / 2.0 + 16.0;
                const double normX = delta.x() / radiusX;
                const double normY = delta.y() / radiusY;
                const double distance = std::hypot(normX, normY);
                if (distance >= 1.0) continue;

                QPointF pushDirection;
                if (distance > 1e-6) {
                    pushDirection = QPointF(normX / distance, normY / distance);
                } else {
                    pushDirection = QPointF(-unit.y(), unit.x());
                }
                const double step =
                    (1.0 - distance) * std::min(radiusX, radiusY) * 0.6;
                result[w] += QPointF(pushDirection.x() * step, pushDirection.y() * step);
                moved = true;
            }
        }
        for (std::size_t i = 0; i < count; ++i) {
            result[i].setX(std::clamp(result[i].x(), margin, width - margin));
            result[i].setY(std::clamp(result[i].y(), margin, height - margin));
        }
        if (!moved) break;
    }

    // 5. 结果兜底：任何非有限值都退回圆心，保证坐标永远可用。
    for (auto& point : result) {
        if (!std::isfinite(point.x()) || !std::isfinite(point.y())) {
            point = QPointF(centerX, centerY);
        }
    }
    return result;
}

}  // namespace gui
