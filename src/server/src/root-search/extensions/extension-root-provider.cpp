#include "root-search/extensions/extension-root-provider.hpp"
#include "actions/extension/extension-actions.hpp"
#include "actions/fallback-actions.hpp"
#include "actions/root-search/root-search-actions.hpp"
#include "clipboard-actions.hpp"
#include "command-actions.hpp"
#include "common.hpp"
#include "extension/extension-command.hpp"
#include "service-registry.hpp"
#include "services/root-search-history/root-search-history-service.hpp"
#include "services/root-item-manager/root-item-manager.hpp"

#include <utility>

namespace {
class RecordCommandHistoryAction : public ProxyAction {
public:
  RecordCommandHistoryAction(AbstractAction *action, EntrypointId commandId)
      : ProxyAction(action), m_commandId(std::move(commandId)) {
    setAutoClose(action->autoClose());
    setStyle(action->style());
    if (auto shortcut = action->shortcut()) addShortcut(*shortcut);
  }

  void executeAfter(ApplicationContext *ctx) override {
    if (auto *history = ctx->services->rootSearchHistory()) history->recordCommand(m_commandId);
  }

private:
  EntrypointId m_commandId;
};
} // namespace

QString CommandRootItem::title() const { return m_command->name(); }

QString CommandRootItem::subtitle() const {
  // Display overriden subtitle if set
  if (auto ext = std::dynamic_pointer_cast<ExtensionCommand>(m_command)) {
    if (ext->overriddenSubtitle() && !ext->overriddenSubtitle()->isEmpty()) {
      return *ext->overriddenSubtitle();
    }
  }
  return m_command->repositoryDisplayName();
}

ImageURL CommandRootItem::iconUrl() const { return m_command->iconUrl(); }
ArgumentList CommandRootItem::arguments() const { return m_command->arguments(); }
bool CommandRootItem::isSuitableForFallback() const { return m_command->isFallback(); }
double CommandRootItem::baseScoreWeight() const { return 1.1; }
QString CommandRootItem::typeDisplayName() const { return tr("Command"); }

std::unique_ptr<ActionPanelState> CommandRootItem::newActionPanel(ApplicationContext *ctx,
                                                                  const RootItemMetadata &metadata) const {
  auto panel = std::make_unique<ListActionPanelState>();
  auto open = new OpenBuiltinCommandAction(m_command, tr("Open command"));
  auto mainSection = panel->createSection();
  auto itemSection = panel->createSection();
  auto extensionSection = panel->createSection();
  auto dangerSection = panel->createSection();

  mainSection->addAction(open);

  for (const auto action : RootSearchActionGenerator::generateActions(*this, metadata)) {
    if (action->isSubmenu()) {
      itemSection->addAction(action);
    } else {
      itemSection->addAction(new RecordCommandHistoryAction(action, uniqueId()));
    }
  }

  if (auto cmd = dynamic_cast<ExtensionCommand *>(m_command.get())) {
    auto copyLocation = new CopyToClipboardAction(
        Clipboard::Text(QString::fromStdString(cmd->path().string())), tr("Copy extension path"));

    extensionSection->addAction(new RecordCommandHistoryAction(copyLocation, uniqueId()));
    dangerSection->addAction(
        new RecordCommandHistoryAction(new UninstallExtensionAction(cmd->extensionId()), uniqueId()));
  }

  return panel;
}

std::unique_ptr<ActionPanelState>
CommandRootItem::fallbackActionPanel(ApplicationContext *ctx, const RootItemMetadata &metadata) const {
  auto panel = std::make_unique<ListActionPanelState>();
  auto main = panel->createSection();
  auto open = new OpenBuiltinCommandAction(m_command, tr("Open command"), "");
  auto manage = new ManageFallbackActions;

  open->setForwardSearchText(true);
  manage->setShortcut(Keyboard::Shortcut::submit());

  main->addAction(open);
  main->addAction(manage);

  return panel;
}

EntrypointId CommandRootItem::uniqueId() const { return m_command->uniqueId(); }

AccessoryList CommandRootItem::accessories() const {
  if (m_command->isInternal()) { return {{.text = tr("Internal Command"), .color = SemanticColor::Cyan}}; }
  return {{.text = tr("Command"), .color = SemanticColor::TextMuted}};
}

std::vector<std::shared_ptr<RootItem>> ExtensionRootProvider::loadItems() const {
  std::vector<std::shared_ptr<RootItem>> items;

  for (const auto &cmd : m_repo->commands()) {
    items.emplace_back(std::make_shared<CommandRootItem>(cmd));
  }

  return items;
}
