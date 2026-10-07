#pragma once

#include <QColor>
#include <QGraphicsItem>
#include <QGraphicsView>
#include <QHash>
#include <QString>
#include <QVector>

#include <vector>

class QLabel;

namespace gui {

struct GraphNodeData {
    QString id;
    QString label;
    QColor color{"#2F80ED"};
    bool active{true};
};

struct GraphEdgeData {
    QString id;
    QString from;
    QString to;
    QString relation;
    bool active{true};
    double confidence{1.0};
    int priority{3};
};

class GraphEdgeItem;
class GraphView;

// 图谱节点：圆角矩形，宽度随名称自适应，可拖拽微调位置。
class GraphNodeItem : public QGraphicsItem {
public:
    GraphNodeItem(const GraphNodeData& data, GraphView* view);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;

    const QString& nodeId() const { return data_.id; }
    bool isActive() const { return data_.active; }
    void setHighlight(bool highlighted, bool dimmed);
    void attachEdge(GraphEdgeItem* edge) { edges_.push_back(edge); }

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

private:
    GraphNodeData data_;
    GraphView* view_{nullptr};
    std::vector<GraphEdgeItem*> edges_;
    double width_{120.0};
    double height_{32.0};
    bool highlighted_{false};
    bool dimmed_{false};
};

// 图谱边：带箭头直线，线宽随置信度变化，失效边使用灰色虚线。
class GraphEdgeItem : public QGraphicsItem {
public:
    GraphEdgeItem(const GraphEdgeData& data, GraphNodeItem* from, GraphNodeItem* to);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;

    void updateGeometry();
    const GraphEdgeData& data() const { return data_; }
    void setHighlight(bool onNeighbourPath);
    // 除两端节点外的其他节点矩形：关系文字要避开这些矩形，否则会被节点压住看不见。
    void setObstacles(const QVector<QRectF>& obstacles) { obstacles_ = obstacles; }

private:
    GraphEdgeData data_;
    GraphNodeItem* from_{nullptr};
    GraphNodeItem* to_{nullptr};
    QPointF fromPoint_;
    QPointF toPoint_;
    QRectF bounds_;
    bool highlighted_{false};
    QVector<QRectF> obstacles_;
};

// 实体关系画布：滚轮缩放、空白拖拽平移、节点拖拽、点击高亮一跳邻域。
class GraphView : public QGraphicsView {
    Q_OBJECT

public:
    explicit GraphView(QWidget* parent = nullptr);

    void setGraph(const QVector<GraphNodeData>& nodes, const QVector<GraphEdgeData>& edges);
    void clearGraph();
    void relayout();
    void selectNode(const QString& id);
    QString selectedNodeId() const { return selectedId_; }
    void resetZoom();
    QPixmap renderToPixmap();
    void notifyNodeMoved(const QString& id);

signals:
    void nodeSelected(const QString& id);
    void nodeActivated(const QString& id);
    void nodeContextMenuRequested(const QString& id, const QPoint& globalPos);
    void nodeMoved(const QString& id);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void drawBackground(QPainter* painter, const QRectF& rect) override;

private:
    void applyHighlight();
    void refreshEdgeObstacles();
    void updateLegendPosition();
    GraphNodeItem* nodeItemAt(const QPointF& scenePos) const;
    GraphNodeItem* nodeById(const QString& id) const;
    QSizeF canvasSize() const;

    QGraphicsScene* scene_{nullptr};
    QHash<QString, GraphNodeItem*> nodeMap_;
    QVector<GraphEdgeItem*> edgeItems_;
    QVector<GraphNodeData> nodeData_;
    QVector<GraphEdgeData> edgeData_;
    QString selectedId_;
    QLabel* legend_{nullptr};
    bool autoFit_{true};
};

}  // namespace gui
