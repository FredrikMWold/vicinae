#include "custom-store/custom-store-form-view-host.hpp"
#include "builtin_icon.hpp"
#include "service-registry.hpp"
#include "services/toast/toast-service.hpp"
#include "ui/action-pannel/action.hpp"
#include "ui/action-pannel/action-panel-state.hpp"

CustomStoreFormViewHost::CustomStoreFormViewHost(std::shared_ptr<CustomStoreService> service)
    : m_service(std::move(service)) {}

CustomStoreFormViewHost::CustomStoreFormViewHost(std::shared_ptr<CustomStoreService> service,
                                                 custom_store::Store store)
    : m_service(std::move(service)), m_initialStore(std::move(store)) {
  m_name = QString::fromStdString(m_initialStore->name);
  m_url = QString::fromStdString(m_initialStore->canonicalUrl());
  m_branch = QString::fromStdString(m_initialStore->branch);
}

QUrl CustomStoreFormViewHost::qmlComponentUrl() const {
  return QUrl(QStringLiteral("qrc:/Vicinae/CustomStoreFormView.qml"));
}

QVariantMap CustomStoreFormViewHost::qmlProperties() {
  return {{QStringLiteral("host"), QVariant::fromValue(this)}};
}

void CustomStoreFormViewHost::initialize() {
  BaseView::initialize();

  setNavigationTitle(m_initialStore ? tr("Edit Custom Store") : tr("Create Custom Store"));
  setNavigationIcon(ImageURL::builtin(BuiltinIcon::Store).setBackgroundTint(SemanticColor::Accent));

  auto panel = std::make_unique<FormActionPanelState>();
  auto section = panel->createSection();
  section->addAction(new StaticAction(tr("Save Store"), BuiltinIcon::EnterKey, [this]() { submit(); }));
  setActions(std::move(panel));
}

void CustomStoreFormViewHost::submit() {
  m_nameError.clear();
  m_urlError.clear();
  m_branchError.clear();

  if (m_name.trimmed().isEmpty()) m_nameError = tr("Name is required");
  if (m_branch.trimmed().isEmpty()) m_branchError = tr("Branch is required");
  if (const auto repository = custom_store::parseGitHubRepository(m_url.trimmed().toStdString());
      !repository) {
    m_urlError = QString::fromStdString(repository.error());
  }

  emit errorsChanged();
  if (!m_nameError.isEmpty() || !m_urlError.isEmpty() || !m_branchError.isEmpty()) {
    context()->services->toastService()->failure(tr("Validation failed"));
    return;
  }

  std::expected<custom_store::Store, std::string> result =
      m_initialStore
          ? m_service->updateStore(QString::fromStdString(m_initialStore->id), m_name, m_url, m_branch)
          : m_service->createStore(m_name, m_url, m_branch);

  if (!result) {
    context()->services->toastService()->failure(QString::fromStdString(result.error()));
    return;
  }

  context()->services->toastService()->success(m_initialStore ? tr("Store updated") : tr("Store created"));
  popSelf();
}

void CustomStoreFormViewHost::setName(const QString &value) {
  if (m_name == value) return;
  m_name = value;
  emit formChanged();
}

void CustomStoreFormViewHost::setUrl(const QString &value) {
  if (m_url == value) return;
  m_url = value;
  emit formChanged();
}

void CustomStoreFormViewHost::setBranch(const QString &value) {
  if (m_branch == value) return;
  m_branch = value;
  emit formChanged();
}