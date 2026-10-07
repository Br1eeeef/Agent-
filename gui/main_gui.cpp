#include "AppContext.h"
#include "MainWindow.h"
#include "pages/GraphPage.h"
#include "pages/MemoryPage.h"
#include "pages/RecallPage.h"
#include "theme/Theme.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QStringList>
#include <QTextStream>

namespace {

// 无头截图模式：--screenshot <目录>
// 依次渲染五个页面并保存 PNG，用于交付材料与渲染自检；不会写回 data/。
int runScreenshotMode(QApplication& application, gui::AppContext& context,
                      const QString& outputDir) {
    QDir().mkpath(outputDir);
    gui::MainWindow window(&context);
    window.resize(1440, 900);
    window.show();
    application.processEvents();

    const QStringList names = {QStringLiteral("01-overview"), QStringLiteral("02-memory"),
                               QStringLiteral("03-recall"), QStringLiteral("04-graph"),
                               QStringLiteral("05-status")};

    for (int index = 0; index < names.size(); ++index) {
        window.activatePage(index);
        application.processEvents();

        // 让每个页面处于有内容的状态，截图才具备演示价值。
        if (index == 1) {
            const auto all = context.manager().all();
            if (!all.empty()) {
                window.memoryPage()->selectMemory(QString::fromStdString(all.front().id));
            }
        } else if (index == 2) {
            window.recallPage()->setQuery(QStringLiteral("微积分 复习"));
        } else if (index == 3) {
            const auto entities = context.graph().entities();
            if (!entities.empty()) {
                window.graphPage()->focusEntity(QString::fromStdString(entities.front().id));
            }
        }

        application.processEvents();
        const QString path = QDir(outputDir).filePath(names.at(index) + QStringLiteral(".png"));
        if (!window.grab().save(path)) {
            QTextStream(stderr) << "截图失败：" << path << '\n';
            return 2;
        }
        QTextStream(stdout) << "已保存 " << path << '\n';
    }
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Agent 记忆数据库演示"));
    QApplication::setOrganizationName(QStringLiteral("CourseDesign"));

    application.setFont(gui::theme::baseFont(10));

    QFile styleFile(QStringLiteral(":/resources/style.qss"));
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&styleFile);
        application.setStyleSheet(stream.readAll());
    }

    gui::AppContext context;

    const QStringList arguments = application.arguments();
    const int screenshotIndex = arguments.indexOf(QStringLiteral("--screenshot"));
    if (screenshotIndex >= 0 && screenshotIndex + 1 < arguments.size()) {
        return runScreenshotMode(application, context, arguments.at(screenshotIndex + 1));
    }

    gui::MainWindow window(&context);
    window.show();
    return application.exec();
}
