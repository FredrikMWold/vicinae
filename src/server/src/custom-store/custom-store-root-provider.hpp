#pragma once

#include "custom-store/custom-store-service.hpp"
#include "services/root-item-manager/root-item-manager.hpp"
#include <filesystem>
#include <memory>

class CustomStoreRootProvider : public RootProvider {
  Q_OBJECT

public:
  explicit CustomStoreRootProvider(const std::filesystem::path &dataDirectory);

  std::vector<std::shared_ptr<RootItem>> loadItems() const override;
  QString uniqueId() const override;
  QString displayName() const override;
  QString description() const override;
  ImageURL icon() const override;
  Type type() const override;

private:
  std::shared_ptr<CustomStoreService> m_service;
};