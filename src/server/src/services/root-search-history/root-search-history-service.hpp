#pragma once

#include "common/entrypoint.hpp"

#include <QObject>
#include <filesystem>
#include <vector>

class LocalStorageService;

struct RootSearchHistoryEntry {
  enum class Kind : std::uint8_t { Command, File };

  Kind kind = Kind::Command;
  EntrypointId commandId;
  std::filesystem::path filePath;
  qint64 lastUsedAt = 0;
  int useCount = 0;
};

class RootSearchHistoryService : public QObject {
  Q_OBJECT

public:
  explicit RootSearchHistoryService(LocalStorageService &storage, QObject *parent = nullptr);

  const std::vector<RootSearchHistoryEntry> &entries() const { return m_entries; }
  bool recordCommand(const EntrypointId &commandId);
  bool recordFile(const std::filesystem::path &path);
  bool removeCommand(const EntrypointId &commandId);
  bool removeFile(const std::filesystem::path &path);
  bool clear();

signals:
  void historyChanged();

private:
  void load();
  void save();
  static bool shouldRecord(const EntrypointId &commandId);
  static bool shouldRecord(const std::filesystem::path &path);
  void sortEntries();

  LocalStorageService &m_storage;
  std::vector<RootSearchHistoryEntry> m_entries;
};