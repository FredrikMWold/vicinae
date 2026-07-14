#include "search-history-view-host.hpp"

#include "clipboard-actions.hpp"
#include "command-actions.hpp"
#include "root-search/extensions/extension-root-provider.hpp"
#include "service-registry.hpp"
#include "services/root-item-manager/root-item-manager.hpp"
#include "actions/root-search/root-search-actions.hpp"
#include "ui/action-pannel/action-panel-state.hpp"
#include "utils/file-list-item.hpp"

#include <QDateTime>
#include <QString>
#include <utility>

namespace {
QString usageTextFor(const RootSearchHistoryEntry &entry) {
  auto when = QDateTime::fromMSecsSinceEpoch(entry.lastUsedAt);
  auto text =
      entry.useCount == 1 ? QStringLiteral("Used once") : QStringLiteral("Used %1 times").arg(entry.useCount);

  if (when.isValid()) text += QStringLiteral(" - ") + when.toString(QStringLiteral("yyyy-MM-dd hh:mm"));
  return text;
}

class RemoveHistoryEntryAction : public AbstractAction {
public:
  explicit RemoveHistoryEntryAction(RootSearchHistoryEntry entry)
      : AbstractAction(QStringLiteral("Remove from History"), ImageURL::builtin(BuiltinIcon::Trash)),
        m_entry(std::move(entry)) {}

  void execute(ApplicationContext *ctx) override {
    if (m_entry.kind == RootSearchHistoryEntry::Kind::File) {
      ctx->services->rootSearchHistory()->removeFile(m_entry.filePath);
      return;
    }
    ctx->services->rootSearchHistory()->removeCommand(m_entry.commandId);
  }

private:
  RootSearchHistoryEntry m_entry;
};

class ClearSearchHistoryAction : public AbstractAction {
public:
  ClearSearchHistoryAction()
      : AbstractAction(QStringLiteral("Clear History"), ImageURL::builtin(BuiltinIcon::Trash)) {
    setStyle(AbstractAction::Style::Danger);
  }

  void execute(ApplicationContext *ctx) override { ctx->services->rootSearchHistory()->clear(); }
};
} // namespace

void SearchHistorySection::setEntries(std::vector<RootSearchHistoryEntry> entries) {
  m_entries = std::move(entries);
  rebuildFilter();
  notifyChanged();
}

const RootSearchHistoryEntry *SearchHistorySection::entryAt(int index) const {
  if (index < 0 || std::cmp_greater_equal(index, m_filtered.size())) return nullptr;
  return m_filtered[index];
}

const CommandRootItem *SearchHistorySection::commandItemFor(const RootSearchHistoryEntry &entry) const {
  auto *item = scope().services()->rootItemManager()->findItemById(entry.commandId);
  return dynamic_cast<const CommandRootItem *>(item);
}

QString SearchHistorySection::itemId(int index) const {
  if (auto entry = entryAt(index)) {
    if (entry->kind == RootSearchHistoryEntry::Kind::File) {
      return QStringLiteral("file:") + QString::fromStdString(entry->filePath.string());
    }
    return QString::fromStdString(std::string(entry->commandId));
  }
  return {};
}

QString SearchHistorySection::itemTitle(int index) const {
  if (auto entry = entryAt(index)) {
    if (entry->kind == RootSearchHistoryEntry::Kind::File) {
      auto name = entry->filePath.filename().string();
      return name.empty() ? QString::fromStdString(entry->filePath.string()) : QString::fromStdString(name);
    }
    if (auto *item = commandItemFor(*entry)) return item->title();
  }
  return {};
}

QString SearchHistorySection::itemSubtitle(int index) const {
  if (auto entry = entryAt(index)) {
    if (entry->kind == RootSearchHistoryEntry::Kind::File) {
      return QString::fromStdString(entry->filePath.parent_path().string());
    }
    if (auto *item = commandItemFor(*entry)) return item->subtitle();
  }
  return {};
}

QVariantList SearchHistorySection::itemAccessories(int index) const {
  if (auto entry = entryAt(index)) return qml::textAccessory(usageTextFor(*entry));
  return {};
}

std::optional<ImageURL> SearchHistorySection::itemIcon(int index) const {
  if (auto entry = entryAt(index)) {
    if (entry->kind == RootSearchHistoryEntry::Kind::File) return ImageURL::fileIcon(entry->filePath);
    if (auto *item = commandItemFor(*entry)) return item->iconUrl();
  }
  return ImageURL::builtin(BuiltinIcon::Clock).setBackgroundTint(SemanticColor::Cyan);
}

std::unique_ptr<ActionPanelState> SearchHistorySection::actionPanel(int index) const {
  auto entry = entryAt(index);
  if (!entry) return nullptr;

  if (entry->kind == RootSearchHistoryEntry::Kind::File) {
    auto panel = FileActions::actionPanel(entry->filePath, scope().appContext());
    auto danger = panel->createSection();

    danger->addAction(new RemoveHistoryEntryAction(*entry));
    danger->addAction(new ClearSearchHistoryAction());

    return panel;
  }

  auto *item = commandItemFor(*entry);
  if (!item) return nullptr;

  auto panel = std::make_unique<ListActionPanelState>();
  auto main = panel->createSection();
  auto danger = panel->createSection();

    main->addAction(new OpenBuiltinCommandAction(item->command(), QStringLiteral("Launch Command")));
  main->addAction(
      new CopyToClipboardAction(Clipboard::Text(QString::fromStdString(std::string(entry->commandId))),
                                QStringLiteral("Copy Command ID")));
  main->addAction(new RemoveHistoryEntryAction(*entry));
  danger->addAction(new ClearSearchHistoryAction());

  return panel;
}

void SearchHistorySection::setFilter(std::string_view query) {
  m_filter = QString::fromUtf8(query.data(), static_cast<qsizetype>(query.size())).trimmed();
  rebuildFilter();
}

void SearchHistorySection::rebuildFilter() {
  m_filtered.clear();
  m_filtered.reserve(m_entries.size());

  for (const auto &entry : m_entries) {
    if (entry.kind == RootSearchHistoryEntry::Kind::File) {
      const auto path = QString::fromStdString(entry.filePath.string());
      const auto name = QString::fromStdString(entry.filePath.filename().string());
      const bool matches = m_filter.isEmpty() || path.contains(m_filter, Qt::CaseInsensitive) ||
                           name.contains(m_filter, Qt::CaseInsensitive);
      if (matches) m_filtered.emplace_back(&entry);
      continue;
    }

    auto *item = commandItemFor(entry);
    if (!item) continue;

    const auto id = QString::fromStdString(std::string(entry.commandId));
    const bool matches = m_filter.isEmpty() || item->title().contains(m_filter, Qt::CaseInsensitive) ||
                         item->subtitle().contains(m_filter, Qt::CaseInsensitive) ||
                         id.contains(m_filter, Qt::CaseInsensitive);
    if (matches) m_filtered.emplace_back(&entry);
  }
}

void SearchHistoryViewHost::initialize() {
  ListViewHost::initialize();
  initModel();

  m_history = context()->services->rootSearchHistory();
  model()->addSource(&m_section);
  setSearchPlaceholderText(initialSearchPlaceholderText());

  connect(m_history, &RootSearchHistoryService::historyChanged, this, [this]() { reload(); });
  reload();
}

void SearchHistoryViewHost::loadInitialData() { model()->setFilter(searchText()); }

void SearchHistoryViewHost::reload() {
  m_section.setEntries(m_history->entries());
  model()->setFilter(searchText());
}