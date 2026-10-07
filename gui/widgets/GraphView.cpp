#include "widgets/GraphView.h"

#include "Format.h"
#include "core/GraphLayout.h"
#include "theme/Theme.h"

#include <QFontMetricsF>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QSet>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace gui {
namespace {

constexpr double kNodeHeight = 32.0;
constexpr double kNodePadding = 22.0;
constexpr double kMinNodeWidth = 72.0;
constexpr double kMaxNodeWidth = 200.0;

QString elideTo(const QString& text, double maxWidth) {
    QFontMetricsF metrics(theme::baseFont(10));
    if (metrics.horizontalAdvance(text) <= maxWidth) return text;
    QString clipped = text;
    while (!clipped.isEmpty() &&
           metrics.horizontalAdvance(clipped + QStringLiteral("…")) > maxWidth) {
        clipped.chop(1);
    }
    return clipped + QStringLiteral("…");
}

}  // namespace

// ---------------------------------------------------------------------------
// GraphNodeItem
// ---------------------------------------------------------------------------

GraphNodeItem::GraphNodeItem(const GraphNodeData& data, GraphView* view)
    : data_(data), view_(view) {
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setCursor(Qt::OpenHandCursor);
    setZValue(2.0);

    const QFontMetricsF metrics(theme::baseFont(10));
    const double textWidth = metrics.horizontalAdvance(data_.label);
    width_ = std::clamp(textWidth + kNodePadding * 2.0, kMinNodeWidth, kMaxNodeWidth);
    height_ = kNodeHeight;
}

QRectF GraphNodeItem::boundingRect() const {
    return QRectF(-width_ / 2.0 - 2.0, -height_ / 2.0 - 2.0, width_ + 4.0, height_ + 4.0);
}

void GraphNodeItem::setHighlight(bool highlighted, bool dimmed) {
    highlighted_ = highlighted;
    dimmed_ = dimmed;
    update();
}

QVariant GraphNodeItem::itemChange(GraphicsItemChange change, const QVariant& value) {
    if (change == QGraphicsItem::ItemPositionHasChanged && view_ != nullptr) {
        for (auto* edge : edges_) {
            if (edge != nullptr) edge->updateGeometry();
        }
        view_->notifyNodeMoved(data_.id);
    }
    return QGraphicsItem::itemChange(change, value);
}

void GraphNodeItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setRenderHint(QPainter::Antialiasing, true);

    const QRectF rect = boundingRect().adjusted(2.0, 2.0, -2.0, -2.0);
    QPainterPath path;
    path.addRoundedRect(rect, 9.0, 9.0);

    // 淡化用“颜色向卡片底色混合”实现，而不是给整个图元设透明度：
    // 设透明度会让节点填充变成半透明，从下方穿过的连线就会透出来。
    const QColor surface(theme::surface());
    const double strength = dimmed_ ? 0.4 : 1.0;
    const auto faded = [&](const QColor& color) {
        return QColor(
            static_cast<int>(surface.red() + (color.red() - surface.red()) * strength),
            static_cast<int>(surface.green() + (color.green() - surface.green()) * strength),
            static_cast<int>(surface.blue() + (color.blue() - surface.blue()) * strength));
    };

    // 节点填充必须不透明：半透明时从节点下方穿过的连线会透过色块显示出来，
    // 看起来像把节点和关系文字搅在一起。
    const double tintRatio = data_.active ? 0.16 : 0.07;
    QColor tint = data_.active ? data_.color : QColor(theme::textMuted());
    QColor fill(
        static_cast<int>(tint.red() * tintRatio + surface.red() * (1.0 - tintRatio)),
        static_cast<int>(tint.green() * tintRatio + surface.green() * (1.0 - tintRatio)),
        static_cast<int>(tint.blue() * tintRatio + surface.blue() * (1.0 - tintRatio)));
    painter->fillPath(path, faded(fill));

    QPen pen(data_.active ? data_.color : QColor(theme::textMuted()));
    pen.setWidthF(highlighted_ ? 2.4 : 1.4);
    if (!data_.active) pen.setStyle(Qt::DashLine);
    painter->setPen(QPen(faded(pen.color()), pen.widthF(), pen.style()));
    painter->drawPath(path);

    if (highlighted_) {
        QColor haloColor = data_.color;
        haloColor.setAlphaF(0.38);
        QPen halo(haloColor);
        halo.setWidthF(4.5);
        painter->setPen(halo);
        painter->drawPath(path);
    }

    QFont font = theme::baseFont(10, highlighted_);
    painter->setFont(font);
    painter->setPen(faded(QColor(data_.active ? theme::textPrimary() : theme::textMuted())));
    painter->drawText(rect, Qt::AlignCenter,
                      elideTo(data_.label, width_ - kNodePadding));
}

// ---------------------------------------------------------------------------
// GraphEdgeItem
// ---------------------------------------------------------------------------

GraphEdgeItem::GraphEdgeItem(const GraphEdgeData& data, GraphNodeItem* from,
                             GraphNodeItem* to)
    : data_(data), from_(from), to_(to) {
    setZValue(1.0);
    updateGeometry();
}

void GraphEdgeItem::setHighlight(bool highlighted) {
    highlighted_ = highlighted;
    update();
}

QRectF GraphEdgeItem::boundingRect() const { return bounds_; }

void GraphEdgeItem::updateGeometry() {
    prepareGeometryChange();
    if (from_ == nullptr || to_ == nullptr) {
        bounds_ = QRectF();
        return;
    }
    fromPoint_ = from_->pos();
    toPoint_ = to_->pos();

    // 起点/终点推到节点边界，避免箭头被节点覆盖。
    const QPointF delta = toPoint_ - fromPoint_;
    const double length = std::hypot(delta.x(), delta.y());
    if (length > 1.0) {
        const QPointF unit = delta / length;
        const double fromOffset = qMin(from_->boundingRect().width() / 2.0, length * 0.35);
        const double toOffset = qMin(to_->boundingRect().width() / 2.0, length * 0.35);
        fromPoint_ += unit * fromOffset;
        toPoint_ -= unit * toOffset;
    }
    // 关系文字可能被沿法线推开几十像素，包围盒要留够余量。
    bounds_ = QRectF(fromPoint_, toPoint_).normalized().adjusted(-90, -70, 90, 70);
    update();
}

void GraphEdgeItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setRenderHint(QPainter::Antialiasing, true);

    QColor lineColor = data_.active ? QColor(theme::accent()) : QColor(theme::textMuted());
    if (!data_.active) lineColor.setAlphaF(0.75);
    QPen pen(lineColor);
    pen.setWidthF(highlighted_ ? 2.6 : 1.0 + std::clamp(data_.confidence, 0.0, 1.0) * 1.6);
    if (!data_.active || data_.priority <= 2) pen.setStyle(Qt::DashLine);
    pen.setCapStyle(Qt::RoundCap);
    if (!data_.active) painter->setOpacity(0.7);
    painter->setPen(pen);
    painter->drawLine(fromPoint_, toPoint_);

    // 箭头
    const QPointF delta = toPoint_ - fromPoint_;
    const double length = std::hypot(delta.x(), delta.y());
    if (length > 1.0) {
        const QPointF unit = delta / length;
        const QPointF normal(-unit.y(), unit.x());
        const double arrow = highlighted_ ? 11.0 : 9.0;
        const QPointF tip = toPoint_;
        const QPointF base = tip - unit * arrow;
        QPolygonF head;
        head << tip << (base + normal * arrow * 0.45) << (base - normal * arrow * 0.45);
        painter->setBrush(lineColor);
        painter->drawPolygon(head);
    }

    // 关系文字：加深色底衬，保证在连线与背景之上都可读。
    if (!data_.relation.isEmpty() && length > 46.0) {
        const QPointF direction = (toPoint_ - fromPoint_) / length;
        const QPointF normal(-direction.y(), direction.x());
        const QPointF middle = (fromPoint_ + toPoint_) / 2.0;
        QFont font = theme::baseFont(9);
        painter->setFont(font);
        const QFontMetricsF metrics(font);
        const QString text = data_.relation;

        const auto rectForOffset = [&](double offset) {
            const QPointF center = middle + normal * offset;
            return QRectF(center.x() - metrics.horizontalAdvance(text) / 2.0 - 6.0,
                          center.y() - metrics.height() / 2.0 - 2.0,
                          metrics.horizontalAdvance(text) + 12.0, metrics.height() + 4.0);
        };
        // 先按法线方向让开连线，若仍压到别的节点就继续沿法线两侧试探，
        // 保证关系文字始终看得见。
        const double candidates[] = {10.0, 26.0, -26.0, 42.0, -42.0, 0.0};
        QRectF textRect = rectForOffset(candidates[0]);
        for (double offset : candidates) {
            const QRectF probe = rectForOffset(offset);
            bool blocked = false;
            for (const auto& obstacle : obstacles_) {
                if (obstacle.adjusted(2, 2, -2, -2).intersects(probe)) {
                    blocked = true;
                    break;
                }
            }
            if (!blocked) {
                textRect = probe;
                break;
            }
            textRect = probe;
        }
        QPainterPath back;
        back.addRoundedRect(textRect, 5.0, 5.0);
        QColor background(theme::surface());
        background.setAlphaF(0.92);
        painter->fillPath(back, background);
        painter->setPen(QColor(theme::textSecondary()));
        painter->drawText(textRect, Qt::AlignCenter, text);
    }
}

// ---------------------------------------------------------------------------
// GraphView
// ---------------------------------------------------------------------------

GraphView::GraphView(QWidget* parent) : QGraphicsView(parent) {
    scene_ = new QGraphicsScene(this);
    setScene(scene_);
    setRenderHint(QPainter::Antialiasing, true);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setFrameShape(QFrame::NoFrame);
    setMinimumSize(320, 240);

    legend_ = new QLabel(this);
    legend_->setObjectName(QStringLiteral("graphLegend"));
    legend_->setTextFormat(Qt::RichText);
    legend_->setAttribute(Qt::WA_TransparentForMouseEvents);
    legend_->setStyleSheet(QStringLiteral(
        "QLabel#graphLegend {"
        "  background: rgba(255,255,255,0.92);"
        "  border: 1px solid %1;"
        "  border-radius: 8px;"
        "  padding: 8px 10px;"
        "  color: %2;"
        "  font-size: 9pt;"
        "}").arg(theme::border(), theme::textSecondary()));
    legend_->setText(
        QStringLiteral("<b>图例</b><br>"
                       "<span style='color:#2F80ED'>●</span> 概念　"
                       "<span style='color:#22B8A6'>●</span> 事实<br>"
                       "<span style='color:#7C5CFC'>●</span> 偏好　"
                       "<span style='color:#828282'>●</span> 其他<br>"
                       "实线 = 活跃　虚线 = 失效"));
    legend_->adjustSize();
    legend_->show();
    updateLegendPosition();
}

QSizeF GraphView::canvasSize() const {
    const QSize size = viewport()->size();
    return QSizeF(qMax(480, size.width()), qMax(360, size.height()));
}

void GraphView::clearGraph() {
    scene_->clear();
    nodeMap_.clear();
    edgeItems_.clear();
    nodeData_.clear();
    edgeData_.clear();
    selectedId_.clear();
}

void GraphView::setGraph(const QVector<GraphNodeData>& nodes, const QVector<GraphEdgeData>& edges) {
    nodeData_ = nodes;
    edgeData_ = edges;
    const QString keepSelection = selectedId_;
    clearGraph();
    selectedId_ = keepSelection;

    std::vector<LayoutNode> layoutNodes;
    layoutNodes.reserve(static_cast<std::size_t>(nodes.size()));
    for (const auto& node : nodes) {
        QFontMetricsF metrics(theme::baseFont(10));
        const double width =
            std::clamp(metrics.horizontalAdvance(node.label) + kNodePadding * 2.0,
                       kMinNodeWidth, kMaxNodeWidth);
        layoutNodes.push_back({node.id.toStdString(), width, kNodeHeight});
    }

    // 布局画布：节点多时按所需面积整体放大，再由 resetZoom 一次性缩放到可见范围，
    // 这样节点不会被挤在边界上互相遮挡，也不需要用户手动拉开。
    const QSizeF viewportSize = canvasSize();
    double neededArea = 0.0;
    for (const auto& node : layoutNodes) {
        neededArea += (node.width + 46.0) * (node.height + 30.0) * 2.6;
    }
    const double viewportArea = viewportSize.width() * viewportSize.height();
    const double scale =
        viewportArea > 0.0 ? std::max(1.0, std::sqrt(neededArea / viewportArea)) : 1.0;
    const QSizeF area(viewportSize.width() * scale, viewportSize.height() * scale);
    scene_->setSceneRect(QRectF(0, 0, area.width(), area.height()));

    std::vector<LayoutEdge> layoutEdges;
    layoutEdges.reserve(static_cast<std::size_t>(edges.size()));
    for (const auto& edge : edges) {
        if (!edge.active) continue;  // 失效边不参与布局拉力，避免把节点拉在一起
        layoutEdges.push_back({edge.from.toStdString(), edge.to.toStdString()});
    }

    const std::vector<QPointF> positions =
        GraphLayout::compute(layoutNodes, layoutEdges, area, 300);

    for (int i = 0; i < nodes.size(); ++i) {
        auto* item = new GraphNodeItem(nodes.at(i), this);
        if (i < static_cast<int>(positions.size())) item->setPos(positions[static_cast<std::size_t>(i)]);
        scene_->addItem(item);
        nodeMap_.insert(nodes.at(i).id, item);
    }

    for (const auto& edge : edges) {
        GraphNodeItem* from = nodeById(edge.from);
        GraphNodeItem* to = nodeById(edge.to);
        if (from == nullptr || to == nullptr) continue;
        auto* item = new GraphEdgeItem(edge, from, to);
        from->attachEdge(item);
        to->attachEdge(item);
        scene_->addItem(item);
        edgeItems_.push_back(item);
    }
    refreshEdgeObstacles();

    if (nodeMap_.contains(selectedId_)) {
        applyHighlight();
    } else {
        selectedId_.clear();
        for (auto* edge : edgeItems_) edge->setHighlight(false);
        for (auto* node : nodeMap_) node->setHighlight(false, false);
    }
    autoFit_ = true;
    resetZoom();
    updateLegendPosition();
}

void GraphView::relayout() {
    setGraph(nodeData_, edgeData_);
}

void GraphView::selectNode(const QString& id) {
    selectedId_ = nodeMap_.contains(id) ? id : QString();
    applyHighlight();
    if (!selectedId_.isEmpty()) {
        for (auto* node : nodeMap_) {
            if (node->nodeId() == selectedId_) node->setCursor(Qt::ClosedHandCursor);
        }
    }
}

void GraphView::notifyNodeMoved(const QString& id) {
    // 节点被拖动后位置变了，关系文字要重新避让。
    refreshEdgeObstacles();
    emit nodeMoved(id);
}

void GraphView::refreshEdgeObstacles() {
    for (auto* item : edgeItems_) {
        if (item == nullptr) continue;
        const GraphEdgeData& data = item->data();
        QVector<QRectF> obstacles;
        obstacles.reserve(nodeMap_.size());
        for (auto* node : nodeMap_) {
            if (node == nullptr) continue;
            if (node->nodeId() == data.from || node->nodeId() == data.to) continue;
            obstacles.push_back(node->sceneBoundingRect());
        }
        item->setObstacles(obstacles);
    }
}

void GraphView::applyHighlight() {
    if (selectedId_.isEmpty()) {
        for (auto* node : nodeMap_) node->setHighlight(false, false);
        for (auto* edge : edgeItems_) edge->setHighlight(false);
        return;
    }

    QSet<QString> neighbours;
    neighbours.insert(selectedId_);
    for (const auto& edge : edgeData_) {
        if (edge.from == selectedId_) neighbours.insert(edge.to);
        if (edge.to == selectedId_) neighbours.insert(edge.from);
    }

    for (auto* node : nodeMap_) {
        const bool isFocus = node->nodeId() == selectedId_;
        const bool isNeighbour = neighbours.contains(node->nodeId());
        node->setHighlight(isFocus, !isFocus && !isNeighbour);
    }
    for (auto* edge : edgeItems_) {
        const auto& data = edge->data();
        const bool touching = data.from == selectedId_ || data.to == selectedId_;
        edge->setHighlight(touching);
    }
}

GraphNodeItem* GraphView::nodeById(const QString& id) const {
    const auto it = nodeMap_.constFind(id);
    return it == nodeMap_.constEnd() ? nullptr : it.value();
}

GraphNodeItem* GraphView::nodeItemAt(const QPointF& scenePos) const {
    for (auto* node : nodeMap_) {
        if (node->sceneBoundingRect().contains(scenePos)) return node;
    }
    return nullptr;
}

void GraphView::resetZoom() {
    resetTransform();
    const QRectF bounds = scene_->itemsBoundingRect().adjusted(-40, -40, 40, 40);
    if (bounds.isEmpty()) return;
    const double scaleX = viewport()->width() / bounds.width();
    const double scaleY = viewport()->height() / bounds.height();
    const double fitScale = std::min(scaleX, scaleY);
    // 内容比视口大时缩小以完整显示；内容较小时保持 1:1 并居中，避免节点被放大到失真。
    if (fitScale < 1.0) {
        fitInView(bounds, Qt::KeepAspectRatio);
    } else {
        centerOn(bounds.center());
    }
}

QPixmap GraphView::renderToPixmap() {
    const QRectF target = scene_->itemsBoundingRect().adjusted(-32, -32, 32, 32);
    QPixmap pixmap(target.size().toSize());
    pixmap.fill(QColor(theme::surface()));
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    scene_->render(&painter, pixmap.rect(), target);
    return pixmap;
}

void GraphView::wheelEvent(QWheelEvent* event) {
    const double factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
    const double current = transform().m11();
    const double next = current * factor;
    if (next < 0.25 || next > 4.0) return;
    autoFit_ = false;  // 用户手动缩放过，后续窗口尺寸变化不再自动回正
    scale(factor, factor);
    event->accept();
}

void GraphView::mousePressEvent(QMouseEvent* event) {
    const QPointF scenePos = mapToScene(event->position().toPoint());
    if (GraphNodeItem* node = nodeItemAt(scenePos)) {
        selectNode(node->nodeId());
        emit nodeSelected(node->nodeId());
    } else {
        selectedId_.clear();
        applyHighlight();
        emit nodeSelected(QString());
    }
    QGraphicsView::mousePressEvent(event);

    if (event->button() == Qt::RightButton) {
        if (GraphNodeItem* node = nodeItemAt(scenePos)) {
            emit nodeContextMenuRequested(node->nodeId(), event->globalPosition().toPoint());
        }
    }
}

void GraphView::mouseDoubleClickEvent(QMouseEvent* event) {
    const QPointF scenePos = mapToScene(event->position().toPoint());
    if (GraphNodeItem* node = nodeItemAt(scenePos)) {
        emit nodeActivated(node->nodeId());
        event->accept();
        return;
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

void GraphView::resizeEvent(QResizeEvent* event) {
    QGraphicsView::resizeEvent(event);
    updateLegendPosition();
    if (autoFit_ && !scene_->items().isEmpty()) resetZoom();
}

void GraphView::updateLegendPosition() {
    if (legend_ == nullptr) return;
    legend_->adjustSize();
    legend_->move(width() - legend_->width() - 14, 14);
    legend_->raise();
}

void GraphView::drawBackground(QPainter* painter, const QRectF& rect) {
    painter->fillRect(rect, QColor(QStringLiteral("#FBFCFE")));
    // 细点阵背景，弱化空白感但不干扰节点与连线。
    const int step = 22;
    painter->setPen(QPen(QColor(QStringLiteral("#E9EDF5")), 1.0));
    const int left = static_cast<int>(rect.left()) - (static_cast<int>(rect.left()) % step);
    const int top = static_cast<int>(rect.top()) - (static_cast<int>(rect.top()) % step);
    for (int x = left; x < rect.right(); x += step) {
        for (int y = top; y < rect.bottom(); y += step) {
            painter->drawPoint(x, y);
        }
    }
}

}  // namespace gui
