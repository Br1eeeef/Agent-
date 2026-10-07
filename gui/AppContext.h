#pragma once

#include "Logger.h"
#include "memory/GraphJsonStorage.h"
#include "memory/GraphMemoryService.h"
#include "memory/GraphStore.h"
#include "memory/MemoryManager.h"

#include <QDateTime>
#include <QObject>
#include <QString>

#include <vector>

namespace gui {

// 全局应用上下文：持有核心对象、数据路径与操作日志。
// 所有页面只通过本类读写核心，避免页面之间互相持有引用。
class AppContext : public QObject {
    Q_OBJECT

public:
    // dataDir 为空时使用源码目录下的 data/；测试可传入临时目录以隔离数据文件。
    explicit AppContext(const QString& dataDir = QString(), QObject* parent = nullptr);

    memory::MemoryManager& manager() { return manager_; }
    const memory::MemoryManager& manager() const { return manager_; }
    memory::GraphStore& graph() { return graph_; }
    const memory::GraphStore& graph() const { return graph_; }
    memory::GraphMemoryService& service() { return service_; }
    Logger& logger() { return logger_; }
    const Logger& logger() const { return logger_; }

    QString dataDir() const { return dataDir_; }
    QString memoriesPath() const;
    QString graphPath() const;
    QString sampleMemoriesPath() const;
    QDateTime lastSavedAt() const { return lastSavedAt_; }
    bool isDirty() const { return dirty_; }

    // ---- 记忆操作：统一写日志并发 memoriesChanged ----
    bool addMemory(const memory::Memory& value, QString* evictedId, QString* error);
    bool updateMemory(const QString& id, const QString& content, int importance,
                      const std::vector<std::string>& keywords, QString* error);
    bool removeMemory(const QString& id, QString* error);

    // ---- 图操作 ----
    memory::RememberResult remember(const memory::RememberRequest& request);
    std::string reviseEdge(const std::string& edgeId, const std::string& relation,
                           const std::string& description,
                           const std::vector<std::string>& memoryIds,
                           std::int64_t eventTime, double confidence, int priority);
    bool forgetEntity(const std::string& entityId, const QString& reason);
    bool forgetEdge(const std::string& edgeId, const QString& reason);
    bool resolvePlaceholder(const std::string& placeholderId, const std::string& realEntityId);
    std::string addEntity(const memory::Entity& entity);

    // ---- 检索 ----
    std::vector<memory::MemoryScore> recall(const QString& query, std::size_t k,
                                            const memory::ScoringWeights& weights,
                                            qint64* elapsedMs);
    // 按ID读取并刷新 LRU，供检索结果卡点击使用。
    const memory::Memory* touch(const QString& id);

    // ---- 持久化 ----
    bool saveAll(QString* error);
    bool loadAll(QString* error);
    void seedSampleData();
    void clearAll();

    void log(const QString& action, const QString& target, bool ok,
             const QString& detail, qint64 elapsedMs = 0);

signals:
    void memoriesChanged();
    void graphChanged();
    void logAppended();
    void dataSaved();

private:
    void seedGraphIfEmpty();
    void seedMemoriesFromFile();
    // 示例记忆的时间戳会随播种时间整体平移，让“新鲜度=exp(-ageDays/30)”在演示时可见。
    void shiftSampleTimestamps();

    memory::MemoryManager manager_{20, 50};
    memory::GraphStore graph_;
    memory::GraphMemoryService service_{graph_};
    Logger logger_;
    QString dataDir_;
    QDateTime lastSavedAt_;
    bool dirty_{false};
};

}  // namespace gui
