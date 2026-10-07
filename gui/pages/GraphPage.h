#pragma once

#include "AppContext.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTableView;
class QTabWidget;
class QTextBrowser;

namespace gui {

class EdgeTableModel;
class EntityTableModel;
class GraphView;

// 实体关系页：实体列表 + 自绘力导向图 + 详情/关系/遍历三页签。
class GraphPage : public QWidget {
    Q_OBJECT

public:
    explicit GraphPage(AppContext* context, QWidget* parent = nullptr);

    void refresh();
    void focusEntity(const QString& entityId);
    void createEntity();
    void createRelation();
    void openAdvancedEntry();

signals:
    void toastRequested(const QString& text, bool error);
    void memoryRequested(const QString& memoryId);

private:
    QWidget* buildToolbar();
    QWidget* buildEntityList();
    QWidget* buildCanvas();
    QWidget* buildInspector();
    QWidget* buildDetailTab();
    QWidget* buildRelationTab();
    QWidget* buildTraversalTab();

    void rebuildGraph();
    void updateDetail();
    void editSelectedEntity();
    void invalidateSelectedEntity();
    void mergePlaceholder();
    void reviseSelectedEdge();
    void invalidateSelectedEdge();
    void runBfs();
    void runDfs();
    void exportImage();
    QString selectedEntityId() const;
    QString selectedEdgeId() const;

    AppContext* context_{nullptr};
    EntityTableModel* entityModel_{nullptr};
    EdgeTableModel* edgeModel_{nullptr};
    GraphView* canvas_{nullptr};
    QTableView* entityTable_{nullptr};
    QTableView* edgeTable_{nullptr};
    QTabWidget* inspector_{nullptr};
    QTextBrowser* detailBrowser_{nullptr};
    QPushButton* editEntityButton_{nullptr};
    QPushButton* invalidateEntityButton_{nullptr};
    QPushButton* mergeButton_{nullptr};
    QPushButton* reviseEdgeButton_{nullptr};
    QPushButton* invalidateEdgeButton_{nullptr};
    QLineEdit* entitySearch_{nullptr};
    QComboBox* entityTypeFilter_{nullptr};
    QCheckBox* showInactiveEntities_{nullptr};
    QCheckBox* showInactiveEdges_{nullptr};
    QLabel* traversalStart_{nullptr};
    QSpinBox* depthSpin_{nullptr};
    QSpinBox* limitSpin_{nullptr};
    QCheckBox* undirectedCheck_{nullptr};
    QLineEdit* keywordEdit_{nullptr};
    QListWidget* traversalList_{nullptr};
    QLabel* canvasHint_{nullptr};
    bool suppressSelection_{false};
};

}  // namespace gui
