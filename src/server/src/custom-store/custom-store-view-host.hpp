#pragma once

#include "custom-store/custom-store-model.hpp"
#include "qml/bridge-view.hpp"
#include "qml/section-list-model.hpp"
#include <QFutureWatcher>
#include <memory>

class CustomStoreViewHost : public ViewHostBase {
  Q_OBJECT
  Q_PROPERTY(QObject *listModel READ listModel CONSTANT)

public:
  CustomStoreViewHost(std::shared_ptr<CustomStoreService> service, custom_store::Store store);

  QUrl qmlComponentUrl() const override;
  QVariantMap qmlProperties() override;
  void initialize() override;
  void loadInitialData() override;
  void textChanged(const QString &text) override;
  void onReactivated() override;
  QString initialNavigationTitle() const override;
  ImageURL initialNavigationIcon() const override;

  QObject *listModel() { return &m_model; }

private:
  void refreshStore();
  void loadCachedCatalog();
  void handleRefreshFinished();

  std::shared_ptr<CustomStoreService> m_service;
  custom_store::Store m_store;
  SectionListModel m_model{this};
  CustomStoreSection m_section;
  QFutureWatcher<CustomStoreService::CatalogResult> m_refreshWatcher;
};