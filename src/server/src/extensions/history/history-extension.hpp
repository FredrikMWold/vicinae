#pragma once

#include "command-database.hpp"
#include "qml/search-history-view-host.hpp"
#include "single-view-command-context.hpp"
#include "theme.hpp"

class SearchHistoryCommand : public BuiltinViewCommand<SearchHistoryViewHost> {
  QString id() const override { return "search"; }
  QString name() const override { return "Command History"; }
  QString description() const override { return "Browse previously launched commands"; }
  std::vector<QString> keywords() const override {
    return {"history", "previous commands", "recent commands"};
  }
  ImageURL iconUrl() const override {
    return ImageURL::builtin(BuiltinIcon::Clock).setBackgroundTint(SemanticColor::Cyan);
  }
};

class HistoryExtension : public BuiltinCommandRepository {
  QString id() const override { return "history"; }
  QString displayName() const override { return "History"; }
  QString description() const override { return "Command launch history"; }
  ImageURL iconUrl() const override {
    return ImageURL::builtin(BuiltinIcon::Clock).setBackgroundTint(SemanticColor::Cyan);
  }

public:
  HistoryExtension() { registerCommand<SearchHistoryCommand>(); }
};