#include "MainWindow.h"

#include "Format.h"
#include "pages/GraphPage.h"
#include "pages/MemoryPage.h"
#include "pages/OverviewPage.h"
#include "pages/RecallPage.h"
#include "pages/StatusPage.h"
#include "theme/Theme.h"
#include "widgets/Toast.h"

#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace gui {
namespace {

struct PageMeta {
    const char* title;
    const char* glyph;
    const char* subtitle;
    const char* action;
};

const PageMeta kPages[] = {
    {"总览", "▦", "短期/长期记忆、活跃索引与图谱的整体状态", "录入第一条记忆"},
    {"记忆管理", "▤", "手动录入、编辑、删除、过滤与保存记忆", "新增记忆"},
    {"智能检索", "⌕", "最小堆 Top-K 召回与三项得分构成", "执行检索"},
    {"实体关系", "◈", "实体消解、关系网络、BFS/DFS 与逻辑删除", "新增实体"},
    {"系统状态", "⚙", "容量、负载、LRU 顺序与操作日志", "保存数据"},
};

// 把导航图标画成固定尺寸的位图：不同字符的宽度差异很大，直接写进文本会导致标题参差不齐。
QIcon glyphIcon(const QString& glyph) {
    constexpr int kSize = 18;
    QPixmap pixmap(kSize, kSize);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QColor(theme::textSecondary()));
    painter.setFont(theme::baseFont(11));
    painter.drawText(pixmap.rect(), Qt::AlignCenter, glyph);
    return QIcon(pixmap);
}

}  // namespace

MainWindow::MainWindow(AppContext* context, QWidget* parent)
    : QMainWindow(parent), context_(context) {
    setWindowTitle(QStringLiteral("Agent 记忆数据库 · 演示界面"));
    resize(1280, 820);
    setMinimumSize(1120, 720);

    auto* central = new QWidget(this);
    central->setObjectName(QStringLiteral("central"));
    auto* layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(buildSidebar());

    auto* content = new QWidget(central);
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(20, 16, 20, 18);
    contentLayout->setSpacing(14);
    contentLayout->addWidget(buildHeader());

    stack_ = new QStackedWidget(content);
    overviewPage_ = new OverviewPage(context_, stack_);
    memoryPage_ = new MemoryPage(context_, stack_);
    recallPage_ = new RecallPage(context_, stack_);
    graphPage_ = new GraphPage(context_, stack_);
    statusPage_ = new StatusPage(context_, stack_);
    stack_->addWidget(overviewPage_);
    stack_->addWidget(memoryPage_);
    stack_->addWidget(recallPage_);
    stack_->addWidget(graphPage_);
    stack_->addWidget(statusPage_);
    contentLayout->addWidget(stack_, 1);
    layout->addWidget(content, 1);
    setCentralWidget(central);

    connect(overviewPage_, &OverviewPage::openMemoryRequested, this, [this](const QString& id) {
        switchPage(MemoryPageIndex);
        memoryPage_->selectMemory(id);
    });
    connect(overviewPage_, &OverviewPage::createMemoryRequested, this,
            [this]() { switchPage(MemoryPageIndex); });
    connect(memoryPage_, &MemoryPage::toastRequested, this, &MainWindow::showToast);
    connect(memoryPage_, &MemoryPage::recallRequested, this, [this](const QString& query) {
        switchPage(RecallPageIndex);
        recallPage_->setQuery(query);
    });
    connect(recallPage_, &RecallPage::toastRequested, this, &MainWindow::showToast);
    connect(graphPage_, &GraphPage::toastRequested, this, &MainWindow::showToast);
    connect(graphPage_, &GraphPage::memoryRequested, this, [this](const QString& memoryId) {
        switchPage(MemoryPageIndex);
        memoryPage_->selectMemory(memoryId);
    });
    connect(statusPage_, &StatusPage::toastRequested, this, &MainWindow::showToast);
    connect(context_, &AppContext::dataSaved, this, &MainWindow::updateSidebarFooter);
    connect(context_, &AppContext::logAppended, this, &MainWindow::updateSidebarFooter);

    nav_->setCurrentRow(OverviewPageIndex);
    switchPage(OverviewPageIndex);
    updateSidebarFooter();
}

QWidget* MainWindow::buildSidebar() {
    auto* sidebar = new QFrame(this);
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(200);

    auto* layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(14, 18, 14, 16);
    layout->setSpacing(14);

    auto* brand = new QLabel(sidebar);
    brand->setObjectName(QStringLiteral("brand"));
    brand->setTextFormat(Qt::RichText);
    brand->setText(QStringLiteral("<div style='font-size:13pt; font-weight:600;'>"
                                  "▣ Agent 记忆库</div>"
                                  "<div style='font-size:9pt; color:%1;'>"
                                  "本地记忆数据库 v0.1</div>")
                       .arg(theme::textMuted()));
    layout->addWidget(brand);

    nav_ = new QListWidget(sidebar);
    nav_->setObjectName(QStringLiteral("nav"));
    nav_->setFrameShape(QFrame::NoFrame);
    nav_->setIconSize(QSize(18, 18));
    for (const auto& page : kPages) {
        auto* item = new QListWidgetItem(glyphIcon(QString::fromUtf8(page.glyph)),
                                         QString::fromUtf8(page.title));
        item->setSizeHint(QSize(0, 38));
        nav_->addItem(item);
    }
    layout->addWidget(nav_, 1);
    connect(nav_, &QListWidget::currentRowChanged, this, &MainWindow::switchPage);

    auto* separator = new QFrame(sidebar);
    separator->setFrameShape(QFrame::HLine);
    separator->setObjectName(QStringLiteral("sidebarSeparator"));
    layout->addWidget(separator);

    footerPaths_ = new QLabel(sidebar);
    footerPaths_->setObjectName(QStringLiteral("sidebarFooter"));
    footerPaths_->setWordWrap(true);
    footerPaths_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(footerPaths_);

    footerSaved_ = new QLabel(sidebar);
    footerSaved_->setObjectName(QStringLiteral("sidebarFooter"));
    footerSaved_->setWordWrap(true);
    layout->addWidget(footerSaved_);

    return sidebar;
}

QWidget* MainWindow::buildHeader() {
    auto* header = new QWidget(this);
    header->setFixedHeight(56);
    auto* layout = new QHBoxLayout(header);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* titleColumn = new QVBoxLayout();
    titleColumn->setSpacing(2);
    pageTitle_ = new QLabel(header);
    pageTitle_->setStyleSheet(
        QStringLiteral("QLabel { font-size: 15pt; font-weight: 600; color: %1; }")
            .arg(theme::textPrimary()));
    pageSubtitle_ = new QLabel(header);
    pageSubtitle_->setStyleSheet(
        QStringLiteral("QLabel { color: %1; }").arg(theme::textSecondary()));
    titleColumn->addWidget(pageTitle_);
    titleColumn->addWidget(pageSubtitle_);
    layout->addLayout(titleColumn);
    layout->addStretch(1);

    primaryButton_ = new QPushButton(header);
    primaryButton_->setProperty("variant", "primary");
    primaryButton_->setMinimumWidth(150);
    connect(primaryButton_, &QPushButton::clicked, this, [this]() {
        switch (stack_->currentIndex()) {
            case MemoryPageIndex:
                memoryPage_->createMemory();
                break;
            case RecallPageIndex:
                recallPage_->runRecall();
                break;
            case GraphPageIndex:
                graphPage_->createEntity();
                break;
            case StatusPageIndex: {
                QString error;
                if (context_->saveAll(&error)) {
                    showToast(QStringLiteral("记忆与实体关系图已保存到 data/"), false);
                } else {
                    showToast(QStringLiteral("保存失败：%1").arg(error), true);
                }
                break;
            }
            case OverviewPageIndex:
            default:
                switchPage(MemoryPageIndex);
                memoryPage_->createMemory();
                break;
        }
    });
    layout->addWidget(primaryButton_);
    return header;
}

void MainWindow::switchPage(int index) {
    if (index < 0 || index >= 5) return;
    stack_->setCurrentIndex(index);
    if (nav_->currentRow() != index) nav_->setCurrentRow(index);
    pageTitle_->setText(QString::fromUtf8(kPages[index].title));
    pageSubtitle_->setText(QString::fromUtf8(kPages[index].subtitle));
    updatePrimaryButton();
    if (index == OverviewPageIndex) overviewPage_->refresh();
    if (index == StatusPageIndex) statusPage_->refresh();
    if (index == GraphPageIndex) graphPage_->refresh();
}

void MainWindow::activatePage(int index) { switchPage(index); }

MemoryPage* MainWindow::memoryPage() { return memoryPage_; }

RecallPage* MainWindow::recallPage() { return recallPage_; }

GraphPage* MainWindow::graphPage() { return graphPage_; }

void MainWindow::updatePrimaryButton() {
    const int index = stack_->currentIndex();
    if (index < 0 || index >= 5) return;
    primaryButton_->setText(QString::fromUtf8(kPages[index].action));
}

void MainWindow::updateSidebarFooter() {
    const QFileInfo memoryFile(context_->memoriesPath());
    const QFileInfo graphFile(context_->graphPath());
    footerPaths_->setText(QStringLiteral("data/memories.json　%1 KB\n"
                                         "data/graph.json　%2 KB")
                              .arg(memoryFile.exists() ? memoryFile.size() / 1024 : 0)
                              .arg(graphFile.exists() ? graphFile.size() / 1024 : 0));
    footerPaths_->setToolTip(QStringLiteral("%1\n%2")
                                 .arg(context_->memoriesPath(), context_->graphPath()));
    footerSaved_->setText(
        context_->isDirty()
            ? QStringLiteral("● 有未保存改动\n最后保存 %1")
                  .arg(fmt::dateTimeText(context_->lastSavedAt()))
            : QStringLiteral("与磁盘一致\n最后保存 %1")
                  .arg(fmt::dateTimeText(context_->lastSavedAt())));
}

void MainWindow::showToast(const QString& text, bool error) {
    Toast::show(this, text, error);
}

}  // namespace gui
