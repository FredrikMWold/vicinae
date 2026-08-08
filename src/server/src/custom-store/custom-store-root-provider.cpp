#include "custom-store/custom-store-root-provider.hpp"
#include "custom-store/custom-store-form-view-host.hpp"
#include "custom-store/custom-store-view-host.hpp"
#include "builtin_icon.hpp"
#include "internal/keyboard/keybind.hpp"
#include "navigation-controller.hpp"
#include "service-registry.hpp"
#include "services/toast/toast-service.hpp"
#include "ui/action-pannel/action.hpp"
#include "vicinae.hpp"
#include <QCoreApplication>
#include <QFutureWatcher>

namespace {

QString translate(const char *text) { return QCoreApplication::translate("CustomStoreRootProvider", text); }

ImageURL storeIcon() {
  return ImageURL::builtin(BuiltinIcon::Store).setBackgroundTint(Omnicast::ACCENT_COLOR);
}

class CreateCustomStoreRootItem : public RootItem {
public:
  explicit CreateCustomStoreRootItem(std::shared_ptr<CustomStoreService> service)
      : m_service(std::move(service)) {}

  EntrypointId uniqueId() const override { return {"custom-stores", "create"}; }
  QString title() const override { return translate("Create Store"); }
  QString subtitle() const override { return translate("Add a GitHub extension repository"); }
  QString typeDisplayName() const override { return translate("Custom Store"); }
  ImageURL iconUrl() const override {
    return ImageURL::builtin(BuiltinIcon::PlusCircle).setBackgroundTint(Omnicast::ACCENT_COLOR);
  }
  double baseScoreWeight() const override { return 1.2; }

  std::unique_ptr<ActionPanelState> newActionPanel(ApplicationContext *,
                                                   const RootItemMetadata &) const override {
    auto panel = std::make_unique<ListActionPanelState>();
    auto section = panel->createSection();
    auto action = new StaticAction(translate("Create Store"), BuiltinIcon::Plus,
                                   [service = m_service](ApplicationContext *ctx) {
                                     ctx->navigation->pushView(new CustomStoreFormViewHost(service));
                                   });
    action->setPrimary(true);
    section->addAction(action);
    return panel;
  }

private:
  std::shared_ptr<CustomStoreService> m_service;
};

class OpenCustomStoreRootItem : public RootItem {
public:
  OpenCustomStoreRootItem(std::shared_ptr<CustomStoreService> service, custom_store::Store store)
      : m_service(std::move(service)), m_store(std::move(store)) {}

  EntrypointId uniqueId() const override { return {"custom-stores", m_store.id}; }
  QString title() const override { return QString::fromStdString(m_store.name); }
  QString subtitle() const override {
    return QStringLiteral("%1/%2").arg(QString::fromStdString(m_store.owner),
                                       QString::fromStdString(m_store.repository));
  }
  QString typeDisplayName() const override { return translate("Custom Store"); }
  ImageURL iconUrl() const override { return storeIcon(); }
  std::vector<QString> keywords() const override {
    return {QString::fromStdString(m_store.owner), QString::fromStdString(m_store.repository),
            QString::fromStdString(m_store.branch)};
  }
  AccessoryList accessories() const override { return {{.text = translate("Custom Store")}}; }

  std::unique_ptr<ActionPanelState> newActionPanel(ApplicationContext *,
                                                   const RootItemMetadata &) const override {
    auto panel = std::make_unique<ListActionPanelState>();
    auto section = panel->createSection();
    auto danger = panel->createSection();

    auto browse = new StaticAction(translate("Browse Extensions"), BuiltinIcon::Store,
                                   [service = m_service, store = m_store](ApplicationContext *ctx) {
                                     ctx->navigation->pushView(new CustomStoreViewHost(service, store));
                                   });
    browse->setPrimary(true);
    section->addAction(browse);

    auto edit = new StaticAction(translate("Edit Store"), BuiltinIcon::Pencil,
                                 [service = m_service, store = m_store](ApplicationContext *ctx) {
                                   ctx->navigation->pushView(new CustomStoreFormViewHost(service, store));
                                 });
    edit->setShortcut(Keybind::EditAction);
    section->addAction(edit);

    auto refresh = new StaticAction(
        translate("Refresh Store"), BuiltinIcon::ArrowClockwise,
        [service = m_service, store = m_store](ApplicationContext *ctx) {
          auto watcher = new QFutureWatcher<CustomStoreService::CatalogResult>();
          ctx->services->toastService()->dynamic(translate("Refreshing store..."));
          QObject::connect(watcher, &QFutureWatcher<CustomStoreService::CatalogResult>::finished,
                           [watcher, toasts = ctx->services->toastService()]() {
                             const auto result = watcher->result();
                             watcher->deleteLater();
                             if (!result) {
                               toasts->failure(QString::fromStdString(result.error()));
                               return;
                             }
                             toasts->success(translate("Store refreshed"));
                           });
          watcher->setFuture(service->refreshStore(store));
        });
    section->addAction(refresh);

    auto remove =
        new StaticAction(translate("Remove Store"), BuiltinIcon::Trash,
                         [service = m_service, id = m_store.id](ApplicationContext *ctx) {
                           const auto result = service->removeStore(QString::fromStdString(id));
                           if (!result) {
                             ctx->services->toastService()->failure(QString::fromStdString(result.error()));
                             return;
                           }
                           ctx->services->toastService()->success(translate("Store removed"));
                         });
    remove->setShortcut(Keybind::RemoveAction);
    remove->setStyle(AbstractAction::Style::Danger);
    danger->addAction(remove);
    return panel;
  }

private:
  std::shared_ptr<CustomStoreService> m_service;
  custom_store::Store m_store;
};

} // namespace

CustomStoreRootProvider::CustomStoreRootProvider(const std::filesystem::path &dataDirectory)
    : m_service(std::make_shared<CustomStoreService>(dataDirectory)) {
  connect(m_service.get(), &CustomStoreService::storesChanged, this, [this]() { emit itemsChanged(); });
}

std::vector<std::shared_ptr<RootItem>> CustomStoreRootProvider::loadItems() const {
  std::vector<std::shared_ptr<RootItem>> items;
  items.reserve(m_service->stores().size() + 1);
  items.emplace_back(std::make_shared<CreateCustomStoreRootItem>(m_service));
  for (const auto &store : m_service->stores()) {
    items.emplace_back(std::make_shared<OpenCustomStoreRootItem>(m_service, store));
  }
  return items;
}

QString CustomStoreRootProvider::uniqueId() const { return QStringLiteral("custom-stores"); }

QString CustomStoreRootProvider::displayName() const { return tr("Custom Stores"); }

QString CustomStoreRootProvider::description() const {
  return tr("GitHub repositories containing custom extensions");
}

ImageURL CustomStoreRootProvider::icon() const { return storeIcon(); }

RootProvider::Type CustomStoreRootProvider::type() const { return RootProvider::Type::GroupProvider; }