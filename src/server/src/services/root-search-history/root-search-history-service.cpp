#include "root-search-history-service.hpp"
#include "services/local-storage/local-storage-service.hpp"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include <system_error>

namespace {
const QString HISTORY_NAMESPACE = QStringLiteral("root-search-history");
const QString ENTRIES_KEY = QStringLiteral("entries");
} // namespace

RootSearchHistoryService::RootSearchHistoryService(LocalStorageService &storage, QObject *parent)
    : QObject(parent), m_storage(storage) {
  load();
}

bool RootSearchHistoryService::shouldRecord(const EntrypointId &commandId) {
  return !commandId.provider.empty() && !commandId.entrypoint.empty() && commandId.provider != "history";
}

bool RootSearchHistoryService::shouldRecord(const std::filesystem::path &path) { return !path.empty(); }

namespace {
QString sortKeyFor(const RootSearchHistoryEntry &entry) {
  if (entry.kind == RootSearchHistoryEntry::Kind::File) {
    return QString::fromStdString(entry.filePath.string());
  }
  return QString::fromStdString(std::string(entry.commandId));
}
} // namespace

void RootSearchHistoryService::sortEntries() {
  std::ranges::sort(m_entries, [](const auto &lhs, const auto &rhs) {
    if (lhs.lastUsedAt == rhs.lastUsedAt) return sortKeyFor(lhs) < sortKeyFor(rhs);
    return lhs.lastUsedAt > rhs.lastUsedAt;
  });
}

void RootSearchHistoryService::load() {
  m_entries.clear();

  auto doc = m_storage.getItemAsJson(HISTORY_NAMESPACE, ENTRIES_KEY);
  if (!doc.isArray()) return;

  auto items = doc.array();
  m_entries.reserve(static_cast<size_t>(items.size()));

  for (const auto &value : items) {
    if (!value.isObject()) continue;
    auto obj = value.toObject();
    auto kind = obj.value(QStringLiteral("kind")).toString(QStringLiteral("command"));
    if (kind == QStringLiteral("file")) {
      auto path = std::filesystem::path(obj.value(QStringLiteral("path")).toString().toStdString());
      if (!shouldRecord(path)) continue;

      m_entries.emplace_back(RootSearchHistoryEntry{
          .kind = RootSearchHistoryEntry::Kind::File,
          .filePath = std::move(path),
          .lastUsedAt = static_cast<qint64>(obj.value(QStringLiteral("lastUsedAt")).toDouble()),
          .useCount = std::max(1, obj.value(QStringLiteral("useCount")).toInt(1)),
      });
      continue;
    }

    auto commandId = EntrypointId{
        obj.value(QStringLiteral("provider")).toString().toStdString(),
        obj.value(QStringLiteral("entrypoint")).toString().toStdString(),
    };
    if (!shouldRecord(commandId)) continue;

    m_entries.emplace_back(RootSearchHistoryEntry{
        .kind = RootSearchHistoryEntry::Kind::Command,
        .commandId = std::move(commandId),
        .lastUsedAt = static_cast<qint64>(obj.value(QStringLiteral("lastUsedAt")).toDouble()),
        .useCount = std::max(1, obj.value(QStringLiteral("useCount")).toInt(1)),
    });
  }

  sortEntries();
}

void RootSearchHistoryService::save() {
  QJsonArray items;

  for (const auto &entry : m_entries) {
    QJsonObject obj;
    if (entry.kind == RootSearchHistoryEntry::Kind::File) {
      obj.insert(QStringLiteral("kind"), QStringLiteral("file"));
      obj.insert(QStringLiteral("path"), QString::fromStdString(entry.filePath.string()));
    } else {
      obj.insert(QStringLiteral("kind"), QStringLiteral("command"));
      obj.insert(QStringLiteral("provider"), QString::fromStdString(entry.commandId.provider));
      obj.insert(QStringLiteral("entrypoint"), QString::fromStdString(entry.commandId.entrypoint));
    }
    obj.insert(QStringLiteral("lastUsedAt"), static_cast<double>(entry.lastUsedAt));
    obj.insert(QStringLiteral("useCount"), entry.useCount);
    items.push_back(obj);
  }

  m_storage.setItemAsJson(HISTORY_NAMESPACE, ENTRIES_KEY, QJsonDocument(items));
}

bool RootSearchHistoryService::recordCommand(const EntrypointId &commandId) {
  if (!shouldRecord(commandId)) return false;

  auto now = QDateTime::currentMSecsSinceEpoch();
  auto it = std::ranges::find_if(m_entries, [&](const auto &entry) {
    return entry.kind == RootSearchHistoryEntry::Kind::Command && entry.commandId == commandId;
  });

  if (it == m_entries.end()) {
    m_entries.emplace_back(RootSearchHistoryEntry{.kind = RootSearchHistoryEntry::Kind::Command,
                                                  .commandId = commandId,
                                                  .lastUsedAt = now,
                                                  .useCount = 1});
  } else {
    it->lastUsedAt = now;
    ++it->useCount;
  }

  sortEntries();
  save();
  emit historyChanged();
  return true;
}

bool RootSearchHistoryService::recordFile(const std::filesystem::path &path) {
  if (!shouldRecord(path)) return false;

  std::error_code ec;
  auto normalized = std::filesystem::weakly_canonical(path, ec);
  if (ec) normalized = path.lexically_normal();

  auto now = QDateTime::currentMSecsSinceEpoch();
  auto it = std::ranges::find_if(m_entries, [&](const auto &entry) {
    return entry.kind == RootSearchHistoryEntry::Kind::File && entry.filePath == normalized;
  });

  if (it == m_entries.end()) {
    m_entries.emplace_back(RootSearchHistoryEntry{.kind = RootSearchHistoryEntry::Kind::File,
                                                  .filePath = std::move(normalized),
                                                  .lastUsedAt = now,
                                                  .useCount = 1});
  } else {
    it->lastUsedAt = now;
    ++it->useCount;
  }

  sortEntries();
  save();
  emit historyChanged();
  return true;
}

bool RootSearchHistoryService::removeCommand(const EntrypointId &commandId) {
  auto oldSize = m_entries.size();

  std::erase_if(m_entries, [&](const auto &entry) {
    return entry.kind == RootSearchHistoryEntry::Kind::Command && entry.commandId == commandId;
  });
  if (m_entries.size() == oldSize) return false;

  save();
  emit historyChanged();
  return true;
}

bool RootSearchHistoryService::removeFile(const std::filesystem::path &path) {
  auto oldSize = m_entries.size();

  std::erase_if(m_entries, [&](const auto &entry) {
    return entry.kind == RootSearchHistoryEntry::Kind::File && entry.filePath == path;
  });
  if (m_entries.size() == oldSize) return false;

  save();
  emit historyChanged();
  return true;
}

bool RootSearchHistoryService::clear() {
  if (m_entries.empty()) return false;

  m_entries.clear();
  save();
  emit historyChanged();
  return true;
}