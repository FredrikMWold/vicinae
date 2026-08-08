#include "custom-store/custom-store-model.hpp"
#include "custom-store/custom-store-installer.hpp"
#include "actions/extension/extension-actions.hpp"
#include "builtin_icon.hpp"
#include "internal/keyboard/keybind.hpp"
#include "service-registry.hpp"
#include "services/app-service/app-service.hpp"
#include "services/extension-registry/extension-registry.hpp"
#include "services/toast/toast-service.hpp"
#include "ui/action-pannel/action.hpp"
#include <QCoreApplication>
#include <QFutureWatcher>

void CustomStoreSection::setEntries(std::vector<custom_store::CatalogEntry> entries,
                                    const custom_store::Store &store,
                                    std::shared_ptr<CustomStoreService> service,
                                    ExtensionRegistry *registry) {
  m_store = store;
  m_service = std::move(service);

  std::vector<CustomStoreListEntry> listEntries;
  listEntries.reserve(entries.size());
  for (auto &entry : entries) {
    const auto id = QString::fromStdString(custom_store::installId(store, entry.manifest.name.toStdString()));
    listEntries.emplace_back(
        CustomStoreListEntry{.catalogEntry = std::move(entry), .installed = registry->isInstalled(id)});
  }
  setItems(std::move(listEntries));
}

QString CustomStoreSection::sectionName() const {
  return QCoreApplication::translate("CustomStoreSection", "Extensions");
}

QVariant CustomStoreSection::customData(int index, int role) const {
  if (role == IsInstalled) return at(index).installed;
  return {};
}

QHash<int, QByteArray> CustomStoreSection::customRoleNames() const { return {{IsInstalled, "isInstalled"}}; }

QString CustomStoreSection::displayTitle(const CustomStoreListEntry &entry) const {
  return entry.catalogEntry.manifest.title;
}

QString CustomStoreSection::displaySubtitle(const CustomStoreListEntry &entry) const {
  const auto &manifest = entry.catalogEntry.manifest;
  if (manifest.author.isEmpty()) return manifest.description;
  return QStringLiteral("%1 - %2").arg(manifest.author, manifest.description);
}

std::optional<ImageURL> CustomStoreSection::displayIcon(const CustomStoreListEntry &entry) const {
  const auto &catalog = entry.catalogEntry;
  auto fallback = ImageURL::builtin(BuiltinIcon::Hammer).setBackgroundTint(SemanticColor::Cyan);
  if (catalog.manifest.icon.isEmpty()) return fallback;
  return ImageURL::local(catalog.sourcePath / "assets" / catalog.manifest.icon.toStdString())
      .withFallback(fallback);
}

QString CustomStoreSection::displayId(const CustomStoreListEntry &entry) const {
  return QString::fromStdString(
      custom_store::installId(m_store, entry.catalogEntry.manifest.name.toStdString()));
}

std::unique_ptr<ActionPanelState>
CustomStoreSection::buildActionPanel(const CustomStoreListEntry &entry) const {
  auto panel = std::make_unique<ListActionPanelState>();
  auto section = panel->createSection();
  auto danger = panel->createSection();
  const auto installId = QString::fromStdString(
      custom_store::installId(m_store, entry.catalogEntry.manifest.name.toStdString()));

  const auto sourceUrl =
      QStringLiteral("%1/tree/%2/%3")
          .arg(QString::fromStdString(m_store.canonicalUrl()), QString::fromStdString(m_store.branch),
               QString::fromStdString(entry.catalogEntry.folder));
  auto install = new StaticAction(
      entry.installed ? QCoreApplication::translate("CustomStoreSection", "Reinstall Extension")
                      : QCoreApplication::translate("CustomStoreSection", "Install Extension"),
      entry.installed ? BuiltinIcon::ArrowClockwise : BuiltinIcon::Download,
      [service = m_service, store = m_store, catalogEntry = entry.catalogEntry](ApplicationContext *ctx) {
        auto installer =
            new CustomStoreInstaller(*ctx->services->extensionRegistry(), store, catalogEntry, service.get());
        QObject::connect(
            installer, &CustomStoreInstaller::stageChanged,
            [toasts = ctx->services->toastService()](const QString &message) { toasts->dynamic(message); });
        QObject::connect(installer, &CustomStoreInstaller::completed,
                         [toasts = ctx->services->toastService()](bool success, const QString &error) {
                           if (success) {
                             toasts->success(
                                 QCoreApplication::translate("CustomStoreSection", "Extension installed"));
                           } else {
                             toasts->failure(error);
                           }
                         });
        installer->start();
      });
  install->setPrimary(true);
  section->addAction(install);

  auto openSource = new StaticAction(
      QCoreApplication::translate("CustomStoreSection", "Open Source"), BuiltinIcon::Github,
      [sourceUrl](ApplicationContext *ctx) { ctx->services->appDb()->openTarget(sourceUrl); });
  section->addAction(openSource);

  auto refresh = new StaticAction(
      QCoreApplication::translate("CustomStoreSection", "Refresh Store"), BuiltinIcon::ArrowClockwise,
      [service = m_service, store = m_store](ApplicationContext *ctx) {
        auto watcher = new QFutureWatcher<CustomStoreService::CatalogResult>();
        ctx->services->toastService()->dynamic(
            QCoreApplication::translate("CustomStoreSection", "Refreshing store..."));
        QObject::connect(watcher, &QFutureWatcher<CustomStoreService::CatalogResult>::finished,
                         [watcher, toasts = ctx->services->toastService()]() {
                           const auto result = watcher->result();
                           watcher->deleteLater();
                           if (!result) {
                             toasts->failure(QString::fromStdString(result.error()));
                             return;
                           }
                           toasts->success(
                               QCoreApplication::translate("CustomStoreSection", "Store refreshed"));
                         });
        watcher->setFuture(service->refreshStore(store));
      });
  section->addAction(refresh);

  if (entry.installed) {
    auto uninstall = new UninstallExtensionAction(installId);
    uninstall->setShortcut(Keybind::RemoveAction);
    danger->addAction(uninstall);
  }
  return panel;
}