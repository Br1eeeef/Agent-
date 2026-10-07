#pragma once

#include "AppContext.h"

#include <QWidget>

#include <vector>

class QChartView;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QSlider;
class QSpinBox;
class QVBoxLayout;

namespace gui {

class EmptyState;

// 智能检索页：召回参数 + 结果卡片 + 得分构成图 + LRU 顺序演示。
class RecallPage : public QWidget {
    Q_OBJECT

public:
    explicit RecallPage(AppContext* context, QWidget* parent = nullptr);

    void setQuery(const QString& query);
    void runRecall();

signals:
    void toastRequested(const QString& text, bool error);

private:
    QWidget* buildParams();
    QWidget* buildResults();
    void updateWeightLabels();
    void refreshLruPanel();
    void rebuildResultCards(const std::vector<memory::MemoryScore>& hits);
    void rebuildChart(const std::vector<memory::MemoryScore>& hits);
    bool weightsValid() const;
    memory::ScoringWeights currentWeights() const;

    AppContext* context_{nullptr};

    QPlainTextEdit* queryEdit_{nullptr};
    QSpinBox* kSpin_{nullptr};
    QSlider* relevanceSlider_{nullptr};
    QSlider* importanceSlider_{nullptr};
    QSlider* recencySlider_{nullptr};
    QLabel* relevanceLabel_{nullptr};
    QLabel* importanceLabel_{nullptr};
    QLabel* recencyLabel_{nullptr};
    QLabel* weightHint_{nullptr};
    QPushButton* recallButton_{nullptr};
    QLabel* summaryLabel_{nullptr};

    QScrollArea* resultScroll_{nullptr};
    QWidget* resultContainer_{nullptr};
    QVBoxLayout* resultLayout_{nullptr};
    EmptyState* emptyState_{nullptr};

    QChartView* chartView_{nullptr};
    QListWidget* lruList_{nullptr};
    QLabel* lruRecentLabel_{nullptr};

    std::vector<memory::MemoryScore> lastHits_;
};

}  // namespace gui
