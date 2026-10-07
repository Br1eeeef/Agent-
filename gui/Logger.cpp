#include "Logger.h"

namespace gui {

void Logger::append(LogEntry entry) {
    if (!entry.time.isValid()) entry.time = QDateTime::currentDateTime();
    entries_.prepend(entry);
    while (entries_.size() > kMaxEntries) entries_.removeLast();
}

void Logger::clear() { entries_.clear(); }

std::size_t Logger::countOfAction(const QString& action) const {
    std::size_t total = 0;
    for (const auto& entry : entries_) {
        if (entry.action == action) ++total;
    }
    return total;
}

}  // namespace gui
