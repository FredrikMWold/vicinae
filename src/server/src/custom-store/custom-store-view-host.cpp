#include "custom-store/custom-store-view-host.hpp"
#include "service-registry.hpp"
#include "services/extension-registry/extension-registry.hpp"
#include "services/toast/toast-service.hpp"

CustomStoreViewHost::CustomStoreViewHost(std::shared_ptr<CustomStoreService> service,
                                         custom_store::Store store)
    : m_service(std::move(service)), m_store(std::move(store)) {
  connect(&m_refreshWatcher, &QFutureWatcher<CustomStoreService::CatalogResult>::finished, this,
          &CustomStoreViewHost::handleRefreshFinished);
}

QUrl CustomStoreViewHost::qmlComponentUrl() const {
  return QUrl(QStringLiteral("qrc:/Vicinae/CustomStoreListingView.qml"));
}

QVariantMap CustomStoreViewHost::qmlProperties() {
  return {{QStringLiteral("host"), QVariant::fromValue(this)}};
}

void CustomStoreViewHost::initialize() {
  BaseView::initialize();
  m_model.setScope(ViewScope(context(), this));
  m_model.addSource(&m_section);
  setSearchPlaceholderText(tr("Search custom extensions"));

  connect(m_service.get(), &CustomStoreService::snapshotChanged, this, [this](const QString &storeId) {
    if (storeId == QString::fromStdString(m_store.id)) loadCachedCatalog();
  });
  connect(context()->services->extensionRegistry(), &ExtensionRegistry::extensionsChanged, this,
          &CustomStoreViewHost::loadCachedCatalog);
}

void CustomStoreViewHost::loadInitialData() {
  if (m_service->hasSnapshot(m_store)) {
    loadCachedCatalog();
  } else {
    refreshStore();
  }
}

void CustomStoreViewHost::textChanged(const QString &text) { m_model.setFilter(text); }

void CustomStoreViewHost::onReactivated() { m_model.refreshActionPanel(); }

QString CustomStoreViewHost::initialNavigationTitle() const { return QString::fromStdString(m_store.name); }

ImageURL CustomStoreViewHost::initialNavigationIcon() const {
  return ImageURL::builtin(BuiltinIcon::Store).setBackgroundTint(SemanticColor::Accent);
}

void CustomStoreViewHost::refreshStore() {
  if (m_refreshWatcher.isRunning()) return;
  setLoading(true);
  m_refreshWatcher.setFuture(m_service->refreshStore(m_store));
}

void CustomStoreViewHost::loadCachedCatalog() {
  const auto result = m_service->catalog(m_store);
  if (!result) {
    context()->services->toastService()->failure(QString::fromStdString(result.error()));
    return;
  }
  m_section.setEntries(*result, m_store, m_service, context()->services->extensionRegistry());
}

void CustomStoreViewHost::handleRefreshFinished() {
  setLoading(false);
  const auto result = m_refreshWatcher.result();
  if (!result) {
    context()->services->toastService()->failure(QString::fromStdString(result.error()));
    return;
  }
  m_section.setEntries(*result, m_store, m_service, context()->services->extensionRegistry());
}