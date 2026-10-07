#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>

#include <cstddef>

namespace gui {

// 系统状态页展示的一条操作记录。
struct LogEntry {
    QDateTime time;
    QString action;   // add / update / remove / recall / find / save / load / remember / forget / revise
    QString target;   // 对象标识，例如记忆ID或实体ID
    bool ok{true};
    QString detail;   // 成功时的补充说明或失败原因
    qint64 elapsedMs{0};
};

// 界面统一的操作日志：所有页面通过 AppContext 写入，系统状态页只读展示。
class Logger {
public:
    static constexpr int kMaxEntries = 500;

    void append(LogEntry entry);
    void clear();

    const QVector<LogEntry>& entries() const { return entries_; }
    std::size_t countOfAction(const QString& action) const;

private:
    QVector<LogEntry> entries_;
};

}  // namespace gui
