#pragma once

#include "AppContext.h"

#include <QMainWindow>

class QLabel;
class QListWidget;
class QPushButton;
class QStackedWidget;

namespace gui {

class GraphPage;
class MemoryPage;
class OverviewPage;
class RecallPage;
class StatusPage;

// 主窗口：左侧导航 + 页头 + 内容区，负责页面间跳转与全局提示。
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(AppContext* context, QWidget* parent = nullptr);

    // 供截图与演示使用：直接切到指定页面并刷新。
    void activatePage(int index);
    MemoryPage* memoryPage();
    RecallPage* recallPage();
    GraphPage* graphPage();

private:
    enum PageIndex {
        OverviewPageIndex = 0,
        MemoryPageIndex,
        RecallPageIndex,
        GraphPageIndex,
        StatusPageIndex,
    };

    QWidget* buildSidebar();
    QWidget* buildHeader();
    void switchPage(int index);
    void updateSidebarFooter();
    void updatePrimaryButton();
    void showToast(const QString& text, bool error);

    AppContext* context_{nullptr};
    QStackedWidget* stack_{nullptr};
    QListWidget* nav_{nullptr};
    QLabel* pageTitle_{nullptr};
    QLabel* pageSubtitle_{nullptr};
    QPushButton* primaryButton_{nullptr};
    QLabel* footerPaths_{nullptr};
    QLabel* footerSaved_{nullptr};

    OverviewPage* overviewPage_{nullptr};
    MemoryPage* memoryPage_{nullptr};
    RecallPage* recallPage_{nullptr};
    GraphPage* graphPage_{nullptr};
    StatusPage* statusPage_{nullptr};
};

}  // namespace gui
