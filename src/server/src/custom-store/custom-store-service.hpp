#pragma once

#include "custom-store/custom-store-database.hpp"
#include "internal/http-client.hpp"
#include <QObject>
#include <QFuture>
#include <expected>
#include <filesystem>

class CustomStoreService : public QObject {
  Q_OBJECT

signals:
  void storesChanged() const;
  void snapshotChanged(const QString &storeId) const;

public:
  using CatalogResult = std::expected<std::vector<custom_store::CatalogEntry>, std::string>;

  explicit CustomStoreService(const std::filesystem::path &dataDirectory, QObject *parent = nullptr);

  const std::vector<custom_store::Store> &stores() const;
  const custom_store::Store *findById(std::string_view id) const;

  std::expected<custom_store::Store, std::string> createStore(const QString &name, const QString &url,
                                                              const QString &branch);
  std::expected<custom_store::Store, std::string> updateStore(const QString &id, const QString &name,
                                                              const QString &url, const QString &branch);
  std::expected<custom_store::Store, std::string> removeStore(const QString &id);

  bool hasSnapshot(const custom_store::Store &store) const;
  CatalogResult catalog(const custom_store::Store &store) const;
  QFuture<CatalogResult> refreshStore(const custom_store::Store &store);

private:
  std::filesystem::path snapshotPath(const custom_store::Store &store) const;

  CustomStoreDatabase m_database;
  std::filesystem::path m_cacheDirectory;
  http::Client m_http;
};