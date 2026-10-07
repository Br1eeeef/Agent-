#include "pages/RecallPage.h"

#include "Format.h"
#include "theme/Theme.h"
#include "widgets/Card.h"
#include "widgets/EmptyState.h"
#include "widgets/ScoreBar.h"

#include <QBarCategoryAxis>
#include <QBarSet>
#include <QChart>
#include <QChartView>
#include <QHBoxLayout>
#include <QHorizontalStackedBarSeries>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QValueAxis>
#include <QVBoxLayout>

#include <functional>
#include <stdexcept>

namespace gui {
namespace {

// 单条召回结果卡片：点击卡片会调用 MemoryManager::find 触发 LRU 更新。
class ResultCard : public QFrame {
public:
    ResultCard(int rank, const memory::MemoryScore& hit, QWidget* parent)
        : QFrame(parent), id_(QString::fromStdString(hit.memory.id)) {
        setProperty("card", true);
        setCursor(Qt::PointingHandCursor);
        setToolTip(QStringLiteral("点击此卡片：调用 find() 读取该记忆并刷新 LRU 顺序"));

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(14, 12, 14, 12);
        layout->setSpacing(8);

        auto* headerRow = new QHBoxLayout();
        headerRow->setSpacing(8);
        auto* rankLabel = new QLabel(QStringLiteral("#%1").arg(rank), this);
        rankLabel->setFixedSize(34, 24);
        rankLabel->setAlignment(Qt::AlignCenter);
        rankLabel->setStyleSheet(
            QStringLiteral("QLabel { background: %1; color: %2; border-radius: 8px;"
                           " font-weight: 600; }")
                .arg(theme::alpha(theme::accent(), 0.14), theme::accent()));
        headerRow->addWidget(rankLabel);

        auto* idLabel = new QLabel(id_, this);
        idLabel->setStyleSheet(
            QStringLiteral("QLabel { color: %1; font-weight: 600; }")
                .arg(theme::textPrimary()));
        headerRow->addWidget(idLabel);

        auto* typeLabel = new QLabel(theme::memoryTypeLabel(hit.memory.type), this);
        const QColor typeColor = theme::memoryTypeColor(hit.memory.type);
        typeLabel->setStyleSheet(
            QStringLiteral("QLabel { color: %1; background: %2; border-radius: 999px;"
                           " padding: 2px 10px; }")
                .arg(typeColor.name(), theme::alpha(typeColor.name(), 0.14)));
        headerRow->addWidget(typeLabel);
        headerRow->addStretch(1);

        auto* scoreLabel = new QLabel(fmt::number(hit.score, 3), this);
        scoreLabel->setStyleSheet(
            QStringLiteral("QLabel { color: %1; font-size: 17pt; font-weight: 600; }")
                .arg(theme::accent()));
        headerRow->addWidget(scoreLabel);
        layout->addLayout(headerRow);

        auto* contentLabel = new QLabel(QString::fromStdString(hit.memory.content), this);
        contentLabel->setWordWrap(true);
        contentLabel->setStyleSheet(
            QStringLiteral("QLabel { color: %1; }").arg(theme::textPrimary()));
        layout->addWidget(contentLabel);

        auto* keywordLabel = new QLabel(
            QStringLiteral("关键词：%1　重要度：%2")
                .arg(fmt::joinKeywords(hit.memory.keywords, 6),
                     fmt::importanceDots(hit.memory.importance)),
            this);
        keywordLabel->setStyleSheet(
            QStringLiteral("QLabel { color: %1; }").arg(theme::textMuted()));
        layout->addWidget(keywordLabel);

        auto* relevanceBar = new ScoreBar(QStringLiteral("相关度"), this);
        relevanceBar->setValue(hit.relevance);
        relevanceBar->setBarColor(QColor(theme::accent()));
        layout->addWidget(relevanceBar);

        auto* importanceBar = new ScoreBar(QStringLiteral("重要度"), this);
        importanceBar->setValue(hit.importance);
        importanceBar->setBarColor(QColor(theme::warning()));
        layout->addWidget(importanceBar);

        auto* recencyBar = new ScoreBar(QStringLiteral("新鲜度"), this);
        recencyBar->setValue(hit.recency);
        recencyBar->setBarColor(QColor(theme::success()));
        layout->addWidget(recencyBar);

        auto* hint = new QLabel(QStringLiteral("点击卡片读取该记忆（刷新 LRU 顺序）"), this);
        hint->setStyleSheet(QStringLiteral("QLabel { color: %1; }").arg(theme::textMuted()));
        layout->addWidget(hint);
    }

    std::function<void(const QString&)> onActivated;

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (onActivated) onActivated(id_);
        QFrame::mousePressEvent(event);
    }

private:
    QString id_;
};

}  // namespace

RecallPage::RecallPage(AppContext* context, QWidget* parent)
    : QWidget(parent), context_(context) {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(14);

    root->addWidget(buildParams(), 0);
    root->addWidget(buildResults(), 1);

    connect(context_, &AppContext::memoriesChanged, this, &RecallPage::refreshLruPanel);
    refreshLruPanel();
}

QWidget* RecallPage::buildParams() {
    auto* body = new QWidget(this);
    body->setMinimumWidth(300);
    body->setMaximumWidth(370);
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* paramCard = new QWidget(body);
    auto* paramLayout = new QVBoxLayout(paramCard);
    paramLayout->setContentsMargins(0, 0, 0, 0);
    paramLayout->setSpacing(10);

    auto* queryLabel = new QLabel(QStringLiteral("检索输入"), paramCard);
    paramLayout->addWidget(queryLabel);
    queryEdit_ = new QPlainTextEdit(paramCard);
    queryEdit_->setFixedHeight(84);
    queryEdit_->setPlaceholderText(QStringLiteral("例如：微积分 复习"));
    paramLayout->addWidget(queryEdit_);

    auto* kRow = new QHBoxLayout();
    kRow->addWidget(new QLabel(QStringLiteral("Top K"), paramCard));
    kSpin_ = new QSpinBox(paramCard);
    kSpin_->setRange(1, 20);
    kSpin_->setValue(10);
    kRow->addWidget(kSpin_);
    kRow->addStretch(1);
    paramLayout->addLayout(kRow);

    auto makeSliderRow = [&](const QString& title, QSlider*& slider, QLabel*& label,
                             double initial) {
        auto* row = new QVBoxLayout();
        auto* header = new QHBoxLayout();
        header->addWidget(new QLabel(title, paramCard));
        header->addStretch(1);
        label = new QLabel(paramCard);
        label->setStyleSheet(QStringLiteral("QLabel { color: %1; font-weight: 600; }")
                                 .arg(theme::textPrimary()));
        header->addWidget(label);
        row->addLayout(header);
        slider = new QSlider(Qt::Horizontal, paramCard);
        slider->setRange(0, 100);
        slider->setValue(static_cast<int>(initial * 100));
        row->addWidget(slider);
        paramLayout->addLayout(row);
    };
    makeSliderRow(QStringLiteral("相关度权重"), relevanceSlider_, relevanceLabel_, 0.5);
    makeSliderRow(QStringLiteral("重要度权重"), importanceSlider_, importanceLabel_, 0.3);
    makeSliderRow(QStringLiteral("新鲜度权重"), recencySlider_, recencyLabel_, 0.2);

    weightHint_ = new QLabel(paramCard);
    weightHint_->setWordWrap(true);
    weightHint_->setStyleSheet(QStringLiteral("QLabel { color: %1; }").arg(theme::textMuted()));
    paramLayout->addWidget(weightHint_);

    auto* buttonRow = new QHBoxLayout();
    recallButton_ = new QPushButton(QStringLiteral("执行检索"), paramCard);
    recallButton_->setProperty("variant", "primary");
    auto* exampleButton = new QPushButton(QStringLiteral("示例查询"), paramCard);
    auto* resetButton = new QPushButton(QStringLiteral("重置权重"), paramCard);
    buttonRow->addWidget(recallButton_);
    buttonRow->addWidget(exampleButton);
    buttonRow->addWidget(resetButton);
    paramLayout->addLayout(buttonRow);

    const auto onWeightChanged = [this]() { updateWeightLabels(); };
    connect(relevanceSlider_, &QSlider::valueChanged, this, onWeightChanged);
    connect(importanceSlider_, &QSlider::valueChanged, this, onWeightChanged);
    connect(recencySlider_, &QSlider::valueChanged, this, onWeightChanged);
    connect(recallButton_, &QPushButton::clicked, this, &RecallPage::runRecall);
    connect(exampleButton, &QPushButton::clicked, this,
            [this]() { setQuery(QStringLiteral("微积分 复习")); });
    connect(resetButton, &QPushButton::clicked, this, [this]() {
        relevanceSlider_->setValue(50);
        importanceSlider_->setValue(30);
        recencySlider_->setValue(20);
        updateWeightLabels();
    });
    updateWeightLabels();

    layout->addWidget(makeCard(QStringLiteral("召回参数"), paramCard, body));

    auto* lruBody = new QWidget(body);
    auto* lruLayout = new QVBoxLayout(lruBody);
    lruLayout->setContentsMargins(0, 0, 0, 0);
    lruLayout->setSpacing(6);

    lruRecentLabel_ = new QLabel(QStringLiteral("尚未读取任何记忆。"), lruBody);
    lruRecentLabel_->setWordWrap(true);
    lruRecentLabel_->setStyleSheet(
        QStringLiteral("QLabel { color: %1; }").arg(theme::textMuted()));
    lruLayout->addWidget(lruRecentLabel_);

    lruList_ = new QListWidget(lruBody);
    lruList_->setObjectName(QStringLiteral("lruList"));
    lruList_->setFrameShape(QFrame::NoFrame);
    lruLayout->addWidget(lruList_, 1);
    layout->addWidget(makeCard(QStringLiteral("LRU 顺序（最近访问在前）"), lruBody, body), 1);

    return body;
}

QWidget* RecallPage::buildResults() {
    auto* body = new QWidget(this);
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    summaryLabel_ = new QLabel(QStringLiteral("尚未执行检索。"), body);
    summaryLabel_->setStyleSheet(
        QStringLiteral("QLabel { color: %1; }").arg(theme::textSecondary()));
    layout->addWidget(summaryLabel_);

    auto* splitter = new QSplitter(Qt::Vertical, body);

    resultScroll_ = new QScrollArea(splitter);
    resultScroll_->setWidgetResizable(true);
    resultScroll_->setFrameShape(QFrame::NoFrame);
    resultContainer_ = new QWidget(resultScroll_);
    resultLayout_ = new QVBoxLayout(resultContainer_);
    resultLayout_->setContentsMargins(0, 0, 8, 0);
    resultLayout_->setSpacing(10);
    resultScroll_->setWidget(resultContainer_);

    emptyState_ = new EmptyState(resultContainer_);
    emptyState_->setText(QStringLiteral("还没有检索结果"),
                         QStringLiteral("在左侧输入问题并点击「执行检索」。"));
    emptyState_->setActionText(QStringLiteral("使用示例查询"));
    emptyState_->setActionVisible(true);
    connect(emptyState_, &EmptyState::actionTriggered, this,
            [this]() { setQuery(QStringLiteral("微积分 复习")); });
    resultLayout_->addWidget(emptyState_);
    resultLayout_->addStretch(1);

    splitter->addWidget(makeCard(QStringLiteral("召回结果"), resultScroll_, splitter));

    chartView_ = new QChartView(splitter);
    chartView_->setRenderHint(QPainter::Antialiasing, true);
    chartView_->setBackgroundBrush(Qt::NoBrush);
    chartView_->setFrameShape(QFrame::NoFrame);
    chartView_->setStyleSheet(QStringLiteral("background: transparent;"));
    chartView_->viewport()->setAutoFillBackground(false);
    chartView_->setMinimumHeight(200);
    splitter->addWidget(makeCard(QStringLiteral("Top-5 得分构成"), chartView_, splitter));
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    layout->addWidget(splitter, 1);
    return body;
}

bool RecallPage::weightsValid() const {
    const double total = relevanceSlider_->value() + importanceSlider_->value() +
                         recencySlider_->value();
    return total > 0.0;
}

memory::ScoringWeights RecallPage::currentWeights() const {
    memory::ScoringWeights weights;
    const double total = relevanceSlider_->value() + importanceSlider_->value() +
                         recencySlider_->value();
    if (total <= 0.0) return weights;
    weights.relevance = relevanceSlider_->value() / total;
    weights.importance = importanceSlider_->value() / total;
    weights.recency = recencySlider_->value() / total;
    return weights;
}

void RecallPage::updateWeightLabels() {
    const double total = relevanceSlider_->value() + importanceSlider_->value() +
                         recencySlider_->value();
    const bool valid = total > 0.0;
    const auto text = [&](int value) {
        if (!valid) return QStringLiteral("%1 → —").arg(value / 100.0, 0, 'f', 2);
        return QStringLiteral("%1 → %2")
            .arg(value / 100.0, 0, 'f', 2)
            .arg(value / total, 0, 'f', 2);
    };
    relevanceLabel_->setText(text(relevanceSlider_->value()));
    importanceLabel_->setText(text(importanceSlider_->value()));
    recencyLabel_->setText(text(recencySlider_->value()));
    weightHint_->setText(
        valid ? QStringLiteral("三项权重自动归一化，显示“滑块值 → 实际权重”。")
              : QStringLiteral("三项权重不能同时为 0，请至少保留一项。"));
    recallButton_->setEnabled(valid);
}

void RecallPage::setQuery(const QString& query) {
    queryEdit_->setPlainText(query);
    runRecall();
}

void RecallPage::runRecall() {
    if (!weightsValid()) {
        emit toastRequested(QStringLiteral("三项权重不能同时为 0"), true);
        return;
    }
    const QString query = queryEdit_->toPlainText().trimmed();
    if (query.isEmpty()) {
        emit toastRequested(QStringLiteral("请输入检索内容"), true);
        return;
    }
    qint64 elapsed = 0;
    try {
        const auto hits = context_->recall(query, static_cast<std::size_t>(kSpin_->value()),
                                           currentWeights(), &elapsed);
        lastHits_ = hits;
        rebuildResultCards(hits);
        rebuildChart(hits);
        summaryLabel_->setText(QStringLiteral("查询「%1」命中 %2 条，耗时 %3 ms（最小堆 Top-K）")
                                   .arg(query)
                                   .arg(hits.size())
                                   .arg(elapsed));
    } catch (const std::exception& ex) {
        emit toastRequested(QStringLiteral("检索失败：%1").arg(QString::fromUtf8(ex.what())),
                            true);
    }
}

void RecallPage::rebuildResultCards(const std::vector<memory::MemoryScore>& hits) {
    while (QLayoutItem* item = resultLayout_->takeAt(0)) {
        if (item->widget() != nullptr && item->widget() != emptyState_) {
            item->widget()->hide();
            item->widget()->setParent(nullptr);
            item->widget()->deleteLater();
        }
        delete item;
    }
    emptyState_->setVisible(hits.empty());
    resultLayout_->addWidget(emptyState_);

    int rank = 1;
    for (const auto& hit : hits) {
        auto* card = new ResultCard(rank, hit, resultContainer_);
        card->onActivated = [this](const QString& id) {
            const memory::Memory* value = context_->touch(id);
            if (value != nullptr) {
                // 反馈放在 LRU 面板里，不再弹右下角提示，避免每次点击都刷屏。
                lruRecentLabel_->setText(
                    QStringLiteral("最近读取：%1（已移到 LRU 第 1 位）").arg(id));
            }
        };
        resultLayout_->addWidget(card);
        ++rank;
    }
    resultLayout_->addStretch(1);
    refreshLruPanel();
}

void RecallPage::rebuildChart(const std::vector<memory::MemoryScore>& hits) {
    auto* relevanceSet = new QBarSet(QStringLiteral("相关度"));
    auto* importanceSet = new QBarSet(QStringLiteral("重要度"));
    auto* recencySet = new QBarSet(QStringLiteral("新鲜度"));
    relevanceSet->setColor(QColor(theme::accent()));
    importanceSet->setColor(QColor(theme::warning()));
    recencySet->setColor(QColor(theme::success()));

    QStringList categories;
    const int limit = qMin<int>(5, static_cast<int>(hits.size()));
    for (int i = 0; i < limit; ++i) {
        const auto& hit = hits[static_cast<std::size_t>(i)];
        *relevanceSet << hit.relevance;
        *importanceSet << hit.importance;
        *recencySet << hit.recency;
        categories << QString::fromStdString(hit.memory.id);
    }

    auto* series = new QHorizontalStackedBarSeries();
    series->append(relevanceSet);
    series->append(importanceSet);
    series->append(recencySet);

    auto* chart = new QChart();
    chart->addSeries(series);
    chart->setBackgroundVisible(false);
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);
    chart->legend()->setLabelColor(QColor(theme::textSecondary()));
    chart->setMargins(QMargins(4, 4, 4, 4));

    auto* axisY = new QBarCategoryAxis();
    axisY->append(categories.isEmpty() ? QStringList{QStringLiteral("无结果")} : categories);
    axisY->setLabelsColor(QColor(theme::textSecondary()));
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    auto* axisX = new QValueAxis();
    axisX->setRange(0.0, 1.0);
    axisX->setLabelFormat("%.1f");
    axisX->setLabelsColor(QColor(theme::textMuted()));
    axisX->setGridLineColor(QColor(theme::separator()));
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    chartView_->setChart(chart);
}

void RecallPage::refreshLruPanel() {
    if (context_ == nullptr || lruList_ == nullptr) return;
    lruList_->clear();
    const auto order = context_->manager().lruOrder();
    const int capacity = static_cast<int>(context_->manager().lruCapacity());
    int index = 1;
    for (const auto& id : order) {
        auto* item = new QListWidgetItem(
            QStringLiteral("%1. %2　（距淘汰还有 %3 位）")
                .arg(index)
                .arg(QString::fromStdString(id))
                .arg(qMax(0, capacity - index)));
        item->setForeground(QColor(index <= 3 ? theme::textPrimary() : theme::textSecondary()));
        lruList_->addItem(item);
        ++index;
    }
    if (order.empty()) {
        lruList_->addItem(QStringLiteral("暂无访问记录：点击检索结果卡片会先写入这里。"));
    }
}

}  // namespace gui
