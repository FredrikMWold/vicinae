#pragma once

#include "custom-store/custom-store-types.hpp"
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

class CustomStoreDatabase {
public:
  explicit CustomStoreDatabase(const std::filesystem::path &path);

  const std::vector<custom_store::Store> &stores() const;
  const custom_store::Store *findById(std::string_view id) const;

  std::expected<custom_store::Store, std::string>
  addStore(std::string_view name, const custom_store::GitHubRepository &repository, std::string_view branch);
  std::expected<custom_store::Store, std::string>
  updateStore(std::string_view id, std::string_view name, const custom_store::GitHubRepository &repository,
              std::string_view branch);
  std::expected<custom_store::Store, std::string> removeStore(std::string_view id);

private:
  std::expected<std::vector<custom_store::Store>, std::string> loadStores();
  std::expected<void, std::string> saveStores(std::span<const custom_store::Store> stores);
  bool containsRepository(const custom_store::GitHubRepository &repository, std::string_view branch,
                          std::string_view excludedId = {}) const;

  std::string m_buffer;
  std::filesystem::path m_path;
  std::vector<custom_store::Store> m_stores;
};