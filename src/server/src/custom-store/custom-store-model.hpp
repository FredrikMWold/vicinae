#pragma once

#include "custom-store/custom-store-service.hpp"
#include "qml/fuzzy-section.hpp"
#include <memory>

class ExtensionRegistry;

struct CustomStoreListEntry {
  custom_store::CatalogEntry catalogEntry;
  bool installed = false;
};

template <> struct fuzzy::FuzzySearchable<CustomStoreListEntry> {
  static fuzzy::Match score(const CustomStoreListEntry &entry, const fuzzy::Query &query) {
    const auto &manifest = entry.catalogEntry.manifest;
    return fuzzy::scoreWeighted({{manifest.title.toStdString(), 1.0},
                                 {manifest.author.toStdString(), 0.5},
                                 {entry.catalogEntry.folder, 0.4},
                                 {manifest.description.toStdString(), 0.3}},
                                query);
  }
};

class CustomStoreSection : public FuzzySection<CustomStoreListEntry> {
public:
  enum ExtraRole { IsInstalled = 100 };

  void setEntries(std::vector<custom_store::CatalogEntry> entries, const custom_store::Store &store,
                  std::shared_ptr<CustomStoreService> service, ExtensionRegistry *registry);

  QString sectionName() const override;
  QVariant customData(int index, int role) const override;
  QHash<int, QByteArray> customRoleNames() const override;

protected:
  QString displayTitle(const CustomStoreListEntry &entry) const override;
  QString displaySubtitle(const CustomStoreListEntry &entry) const override;
  std::optional<ImageURL> displayIcon(const CustomStoreListEntry &entry) const override;
  QString displayId(const CustomStoreListEntry &entry) const override;
  std::unique_ptr<ActionPanelState> buildActionPanel(const CustomStoreListEntry &entry) const override;

private:
  custom_store::Store m_store;
  std::shared_ptr<CustomStoreService> m_service;
};