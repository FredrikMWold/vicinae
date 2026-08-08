#pragma once

#include "list-view-host.hpp"
#include "section-source.hpp"
#include "services/root-search-history/root-search-history-service.hpp"

#include <vector>

class CommandRootItem;

class SearchHistorySection : public SectionSource {
public:
  void setEntries(std::vector<RootSearchHistoryEntry> entries);

  QString sectionName() const override { return QStringLiteral("History"); }
  int count() const override { return static_cast<int>(m_filtered.size()); }
  QString itemId(int index) const override;
  QString itemTitle(int index) const override;
  QString itemSubtitle(int index) const override;
  AccessoryList itemAccessories(int index) const override;
  std::optional<ImageURL> itemIcon(int index) const override;
  std::unique_ptr<ActionPanelState> actionPanel(int index) const override;
  void setFilter(std::string_view query) override;

private:
  const RootSearchHistoryEntry *entryAt(int index) const;
  const CommandRootItem *commandItemFor(const RootSearchHistoryEntry &entry) const;
  void rebuildFilter();

  std::vector<RootSearchHistoryEntry> m_entries;
  std::vector<const RootSearchHistoryEntry *> m_filtered;
  QString m_filter;
};

class SearchHistoryViewHost : public ListViewHost {
  Q_OBJECT

public:
  void initialize() override;
  void loadInitialData() override;
  QString initialSearchPlaceholderText() const override { return QStringLiteral("Search history..."); }

private:
  void reload();

  SearchHistorySection m_section;
  RootSearchHistoryService *m_history = nullptr;
};