#include "AppContext.h"

#include "theme/Theme.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>

#include <stdexcept>
#include <string>

#include <algorithm>
#include <cstdint>
#include <utility>

namespace gui {
namespace {

// 内置示例记忆：data/sample_memories.json 缺失时的兜底，内容与文件保持一致。
struct BuiltinMemory {
    const char* id;
    const char* content;
    int importance;
    memory::MemoryType type;
};

const BuiltinMemory kBuiltinMemories[] = {
    {"profile-001", "用户是计算机科学与技术专业学生", 5, memory::MemoryType::Profile},
    {"profile-002", "用户通常晚上八点开始学习", 4, memory::MemoryType::Profile},
    {"plan-001", "本周需要复习高等数学第二章", 5, memory::MemoryType::Plan},
    {"event-001", "星期五提交程序设计实践作业", 5, memory::MemoryType::Event},
    {"conversation-001", "用户询问过循环队列如何覆盖最旧元素", 3,
     memory::MemoryType::Conversation},
};

memory::Memory recallable(const char* id, const char* content, int importance,
                          memory::MemoryType type) {
    return memory::Memory::create(id, content, importance, type);
}

}  // namespace

AppContext::AppContext(const QString& dataDir, QObject* parent) : QObject(parent) {
    if (!dataDir.isEmpty()) {
        dataDir_ = dataDir;
    } else {
#ifdef AGENT_MEMORY_SOURCE_DIR
        dataDir_ = QString::fromUtf8(AGENT_MEMORY_SOURCE_DIR) + QStringLiteral("/data");
#else
        dataDir_ = QCoreApplication::applicationDirPath() + QStringLiteral("/data");
#endif
    }
    QDir().mkpath(dataDir_);

    QString error;
    if (!loadAll(&error)) {
        seedSampleData();
        log(QStringLiteral("load"), QStringLiteral("-"), false,
            QStringLiteral("未找到已有数据，已用示例数据初始化：%1").arg(error));
    }
}

QString AppContext::memoriesPath() const {
    return QDir(dataDir_).filePath(QStringLiteral("memories.json"));
}

QString AppContext::graphPath() const {
    return QDir(dataDir_).filePath(QStringLiteral("graph.json"));
}

QString AppContext::sampleMemoriesPath() const {
    return QDir(dataDir_).filePath(QStringLiteral("sample_memories.json"));
}

bool AppContext::addMemory(const memory::Memory& value, QString* evictedId, QString* error) {
    QElapsedTimer timer;
    timer.start();
    try {
        const auto evicted = manager_.add(value);
        if (evictedId != nullptr) {
            *evictedId = evicted.has_value() ? QString::fromStdString(*evicted) : QString();
        }
        dirty_ = true;
        log(QStringLiteral("add"), QString::fromStdString(value.id), true,
            evicted.has_value()
                ? QStringLiteral("短期队列已淘汰 %1").arg(QString::fromStdString(*evicted))
                : QStringLiteral("写入长期记忆与短期队列"),
            timer.elapsed());
        emit memoriesChanged();
        return true;
    } catch (const std::exception& ex) {
        if (error != nullptr) *error = QString::fromUtf8(ex.what());
        log(QStringLiteral("add"), QString::fromStdString(value.id), false,
            QString::fromUtf8(ex.what()), timer.elapsed());
        return false;
    }
}

bool AppContext::updateMemory(const QString& id, const QString& content, int importance,
                              const std::vector<std::string>& keywords, QString* error) {
    QElapsedTimer timer;
    timer.start();
    try {
        const bool ok = manager_.update(id.toStdString(), content.toStdString(), importance,
                                        keywords);
        if (!ok) {
            if (error != nullptr) *error = QStringLiteral("未找到该记忆：%1").arg(id);
            log(QStringLiteral("update"), id, false, QStringLiteral("未找到该记忆"),
                timer.elapsed());
            return false;
        }
        dirty_ = true;
        log(QStringLiteral("update"), id, true, QStringLiteral("已更新内容与重要度"),
            timer.elapsed());
        emit memoriesChanged();
        return true;
    } catch (const std::exception& ex) {
        if (error != nullptr) *error = QString::fromUtf8(ex.what());
        log(QStringLiteral("update"), id, false, QString::fromUtf8(ex.what()), timer.elapsed());
        return false;
    }
}

bool AppContext::removeMemory(const QString& id, QString* error) {
    QElapsedTimer timer;
    timer.start();
    try {
        const bool ok = manager_.remove(id.toStdString());
        if (!ok) {
            if (error != nullptr) *error = QStringLiteral("未找到该记忆：%1").arg(id);
            log(QStringLiteral("remove"), id, false, QStringLiteral("未找到该记忆"),
                timer.elapsed());
            return false;
        }
        dirty_ = true;
        log(QStringLiteral("remove"), id, true,
            QStringLiteral("已从长期记忆移除；短期队列中的残留ID不会同步清理"),
            timer.elapsed());
        emit memoriesChanged();
        return true;
    } catch (const std::exception& ex) {
        if (error != nullptr) *error = QString::fromUtf8(ex.what());
        log(QStringLiteral("remove"), id, false, QString::fromUtf8(ex.what()), timer.elapsed());
        return false;
    }
}

memory::RememberResult AppContext::remember(const memory::RememberRequest& request) {
    QElapsedTimer timer;
    timer.start();
    try {
        const auto result = service_.remember(request);
        dirty_ = true;
        log(QStringLiteral("remember"), QString::fromStdString(result.memoryId), true,
            QStringLiteral("实体 %1 新建 / %2 合并，关系 %3 条")
                .arg(result.createdEntityIds.size())
                .arg(result.mergedEntityIds.size())
                .arg(result.edgeIds.size()),
            timer.elapsed());
        emit memoriesChanged();
        emit graphChanged();
        return result;
    } catch (const std::exception& ex) {
        log(QStringLiteral("remember"), QString::fromStdString(request.memory.id), false,
            QString::fromUtf8(ex.what()), timer.elapsed());
        throw;
    }
}

std::string AppContext::reviseEdge(const std::string& edgeId, const std::string& relation,
                                   const std::string& description,
                                   const std::vector<std::string>& memoryIds,
                                   std::int64_t eventTime, double confidence, int priority) {
    QElapsedTimer timer;
    timer.start();
    const std::string newId =
        service_.reviseEdge(edgeId, relation, description, memoryIds, eventTime, confidence,
                            priority);
    const bool ok = !newId.empty();
    dirty_ = dirty_ || ok;
    log(QStringLiteral("revise"), QString::fromStdString(edgeId), ok,
        ok ? QStringLiteral("旧边失效，新边 %1 已生效").arg(QString::fromStdString(newId))
           : QStringLiteral("未找到该关系"),
        timer.elapsed());
    if (ok) emit graphChanged();
    return newId;
}

bool AppContext::forgetEntity(const std::string& entityId, const QString& reason) {
    QElapsedTimer timer;
    timer.start();
    const bool ok = service_.forgetEntity(entityId, reason.toStdString());
    dirty_ = dirty_ || ok;
    log(QStringLiteral("forget"), QString::fromStdString(entityId), ok,
        ok ? QStringLiteral("实体已逻辑删除，关联边一并失效")
           : QStringLiteral("实体不存在或已失效"),
        timer.elapsed());
    if (ok) emit graphChanged();
    return ok;
}

bool AppContext::forgetEdge(const std::string& edgeId, const QString& reason) {
    QElapsedTimer timer;
    timer.start();
    const bool ok = service_.forgetEdge(edgeId, reason.toStdString());
    dirty_ = dirty_ || ok;
    log(QStringLiteral("forget"), QString::fromStdString(edgeId), ok,
        ok ? QStringLiteral("关系已逻辑删除") : QStringLiteral("关系不存在或已失效"),
        timer.elapsed());
    if (ok) emit graphChanged();
    return ok;
}

bool AppContext::resolvePlaceholder(const std::string& placeholderId,
                                    const std::string& realEntityId) {
    QElapsedTimer timer;
    timer.start();
    const bool ok = service_.resolvePlaceholder(placeholderId, realEntityId);
    dirty_ = dirty_ || ok;
    log(QStringLiteral("resolve"), QString::fromStdString(placeholderId), ok,
        ok ? QStringLiteral("占位实体已并入 %1").arg(QString::fromStdString(realEntityId))
           : QStringLiteral("两者之一不存在或已失效"),
        timer.elapsed());
    if (ok) emit graphChanged();
    return ok;
}

std::string AppContext::addEntity(const memory::Entity& entity) {
    QElapsedTimer timer;
    timer.start();
    try {
        const std::string id = graph_.addEntity(entity);
        dirty_ = true;
        log(QStringLiteral("entity"), QString::fromStdString(id), true,
            QStringLiteral("已写入实体 %1").arg(QString::fromStdString(entity.name)),
            timer.elapsed());
        emit graphChanged();
        return id;
    } catch (const std::exception& ex) {
        log(QStringLiteral("entity"), QString::fromStdString(entity.name), false,
            QString::fromUtf8(ex.what()), timer.elapsed());
        throw;
    }
}

std::vector<memory::MemoryScore> AppContext::recall(const QString& query, std::size_t k,
                                                    const memory::ScoringWeights& weights,
                                                    qint64* elapsedMs) {
    QElapsedTimer timer;
    timer.start();
    std::vector<memory::MemoryScore> hits;
    try {
        hits = manager_.recall(query.toStdString(), k, weights);
        const qint64 elapsed = timer.elapsed();
        if (elapsedMs != nullptr) *elapsedMs = elapsed;
        log(QStringLiteral("recall"), query, true,
            QStringLiteral("K=%1，命中 %2 条").arg(k).arg(hits.size()), elapsed);
    } catch (const std::exception& ex) {
        const qint64 elapsed = timer.elapsed();
        if (elapsedMs != nullptr) *elapsedMs = elapsed;
        log(QStringLiteral("recall"), query, false, QString::fromUtf8(ex.what()), elapsed);
        throw;
    }
    return hits;
}

const memory::Memory* AppContext::touch(const QString& id) {
    memory::Memory* value = manager_.find(id.toStdString());
    if (value != nullptr) {
        log(QStringLiteral("find"), id, true,
            QStringLiteral("命中并刷新 LRU 顺序（当前第 1 位）"));
        emit memoriesChanged();
    }
    return value;
}

bool AppContext::saveAll(QString* error) {
    QElapsedTimer timer;
    timer.start();
    try {
        manager_.save(memoriesPath().toStdString());
        memory::GraphJsonStorage::save(graphPath().toStdString(), graph_);
        lastSavedAt_ = QDateTime::currentDateTime();
        dirty_ = false;
        log(QStringLiteral("save"), memoriesPath(), true,
            QStringLiteral("记忆与实体关系图已写入 data/"), timer.elapsed());
        emit dataSaved();
        return true;
    } catch (const std::exception& ex) {
        if (error != nullptr) *error = QString::fromUtf8(ex.what());
        log(QStringLiteral("save"), memoriesPath(), false, QString::fromUtf8(ex.what()),
            timer.elapsed());
        return false;
    }
}

bool AppContext::loadAll(QString* error) {
    QElapsedTimer timer;
    timer.start();
    const QFileInfo memoriesInfo(memoriesPath());
    if (!memoriesInfo.exists()) {
        if (error != nullptr) *error = QStringLiteral("data/memories.json 不存在");
        return false;
    }
    try {
        manager_.load(memoriesPath().toStdString());
        if (QFileInfo::exists(graphPath())) {
            memory::GraphJsonStorage::load(graphPath().toStdString(), graph_);
        } else {
            seedGraphIfEmpty();
        }
        lastSavedAt_ = memoriesInfo.lastModified();
        dirty_ = false;
        log(QStringLiteral("load"), memoriesPath(), true,
            QStringLiteral("已恢复 %1 条记忆、%2 个实体")
                .arg(manager_.size())
                .arg(graph_.entityCount()),
            timer.elapsed());
        emit memoriesChanged();
        emit graphChanged();
        return true;
    } catch (const std::exception& ex) {
        if (error != nullptr) *error = QString::fromUtf8(ex.what());
        log(QStringLiteral("load"), memoriesPath(), false, QString::fromUtf8(ex.what()),
            timer.elapsed());
        return false;
    }
}

void AppContext::seedMemoriesFromFile() {
    manager_.clear();
    if (QFileInfo::exists(sampleMemoriesPath())) {
        try {
            manager_.load(sampleMemoriesPath().toStdString());
            shiftSampleTimestamps();
            return;
        } catch (const std::exception&) {
            manager_.clear();
        }
    }
    for (const auto& item : kBuiltinMemories) {
        manager_.add(recallable(item.id, item.content, item.importance, item.type));
    }
}

void AppContext::shiftSampleTimestamps() {
    const auto values = manager_.all();
    if (values.empty()) return;
    std::int64_t newest = 0;
    for (const auto& value : values) newest = std::max(newest, value.createdAt);

    const std::int64_t now = memory::unixNow();
    const std::int64_t oneWeek = 7 * 24 * 60 * 60;
    // 只有明显过期的示例数据才平移，避免覆盖用户自己维护的时间线。
    if (newest <= 0 || now - newest <= oneWeek) return;

    const std::int64_t delta = now - newest;
    std::vector<memory::Memory> shifted;
    shifted.reserve(values.size());
    for (auto value : values) {
        value.createdAt += delta;
        value.updatedAt += delta;
        value.lastAccessedAt += delta;
        shifted.push_back(std::move(value));
    }
    std::sort(shifted.begin(), shifted.end(), [](const auto& left, const auto& right) {
        if (left.createdAt != right.createdAt) return left.createdAt < right.createdAt;
        return left.id < right.id;
    });

    manager_.clear();
    for (auto& value : shifted) manager_.add(std::move(value));
}

void AppContext::seedGraphIfEmpty() {
    if (graph_.entityCount() > 0) return;

    auto build = [](const char* id, const char* content, int importance,
                    memory::MemoryType type) {
        memory::RememberRequest request;
        request.memory = recallable(id, content, importance, type);
        return request;
    };

    // A. 计划：重修微积分，并复习高等数学第二章。
    {
        auto request = build("plan-001", "本周需要复习高等数学第二章", 5,
                             memory::MemoryType::Plan);
        request.entities = {
            {"微积分", memory::EntityType::Concept, {"calculus"}, "高等数学基础课程", 5},
            {"用户需要重修微积分", memory::EntityType::Fact, {}, "本学期学习计划", 5},
            {"高等数学第二章", memory::EntityType::Concept, {}, "复习范围", 4},
        };
        request.relations = {
            {"用户需要重修微积分", "微积分", "涉及", "重修计划", memory::EdgeSource::User, 5,
             1.0, 0},
            {"高等数学第二章", "微积分", "属于", "复习范围", memory::EdgeSource::User, 4, 1.0,
             0},
        };
        service_.remember(request);
    }

    // B. 偏好：讲解定理时带上数学史。
    {
        auto request = build("profile-002", "用户通常晚上八点开始学习", 4,
                             memory::MemoryType::Profile);
        request.entities = {
            {"讲解定理时带上数学史", memory::EntityType::Preference, {"数学史"},
             "教学方式偏好", 4},
        };
        request.relations = {
            {"讲解定理时带上数学史", "微积分", "适用于", "教学偏好",
             memory::EdgeSource::User, 4, 1.0, 0},
        };
        service_.remember(request);
    }

    // C. 事件：星期五提交程序设计实践作业。
    {
        auto request =
            build("event-001", "星期五提交程序设计实践作业", 5, memory::MemoryType::Event);
        request.entities = {
            {"程序设计实践作业", memory::EntityType::Fact, {}, "课程作业", 5},
            {"星期五", memory::EntityType::Other, {}, "截止时间", 4},
        };
        request.relations = {
            {"程序设计实践作业", "星期五", "截止于", "作业截止时间",
             memory::EdgeSource::User, 5, 1.0, 0},
        };
        service_.remember(request);
    }

    // D. 画像：计算机科学与技术专业。
    {
        auto request = build("profile-001", "用户是计算机科学与技术专业学生", 5,
                             memory::MemoryType::Profile);
        request.entities = {
            {"计算机科学与技术", memory::EntityType::Concept, {"计科"}, "所学专业", 5},
            {"用户是计算机科学与技术专业学生", memory::EntityType::Fact, {}, "用户画像", 5},
        };
        request.relations = {
            {"用户是计算机科学与技术专业学生", "计算机科学与技术", "就读于", "用户画像",
             memory::EdgeSource::User, 5, 1.0, 0},
        };
        service_.remember(request);
    }

    // E. 对话：循环队列，并顺带演示别名消解（“计科”并入“计算机科学与技术”）。
    {
        auto request = build("conversation-001", "用户询问过循环队列如何覆盖最旧元素", 3,
                             memory::MemoryType::Conversation);
        request.entities = {
            {"循环队列", memory::EntityType::Concept, {"环形缓冲区"}, "数据结构概念", 3},
            {"用户询问过循环队列如何覆盖最旧元素", memory::EntityType::Fact, {}, "历史问答", 3},
            {"计科", memory::EntityType::Concept, {}, "同义写法，用于演示别名消解", 4},
        };
        request.relations = {
            {"用户询问过循环队列如何覆盖最旧元素", "循环队列", "涉及", "历史问答",
             memory::EdgeSource::User, 3, 1.0, 0},
        };
        service_.remember(request);
    }
}

void AppContext::seedSampleData() {
    seedMemoriesFromFile();
    graph_.clear();
    seedGraphIfEmpty();
    dirty_ = true;
    log(QStringLiteral("seed"), QStringLiteral("data/"), true,
        QStringLiteral("已播种 %1 条记忆、%2 个实体、%3 条关系")
            .arg(manager_.size())
            .arg(graph_.entityCount())
            .arg(graph_.edgeCount()));
    emit memoriesChanged();
    emit graphChanged();
}

void AppContext::clearAll() {
    manager_.clear();
    graph_.clear();
    dirty_ = true;
    log(QStringLiteral("clear"), QStringLiteral("*"), true,
        QStringLiteral("已清空内存中的记忆与实体关系图（磁盘文件保留）"));
    emit memoriesChanged();
    emit graphChanged();
}

void AppContext::log(const QString& action, const QString& target, bool ok,
                     const QString& detail, qint64 elapsedMs) {
    LogEntry entry;
    entry.time = QDateTime::currentDateTime();
    entry.action = action;
    entry.target = target;
    entry.ok = ok;
    entry.detail = detail;
    entry.elapsedMs = elapsedMs;
    logger_.append(entry);
    emit logAppended();
}

}  // namespace gui
