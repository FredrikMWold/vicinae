#include "custom-store/custom-store-database.hpp"
#include <QDateTime>
#include <QUuid>
#include <algorithm>
#include <format>
#include <glaze/json/read.hpp>
#include <glaze/json/write.hpp>

namespace fs = std::filesystem;

CustomStoreDatabase::CustomStoreDatabase(const fs::path &path) : m_path(path) {
  if (!fs::is_regular_file(m_path)) {
    fs::create_directories(m_path.parent_path());
    if (const auto result = saveStores({}); !result) {
      qCritical() << "Unable to create custom store database at" << m_path.c_str() << result.error();
    }
  }

  m_stores = loadStores().value_or(std::vector<custom_store::Store>{});
}

const std::vector<custom_store::Store> &CustomStoreDatabase::stores() const { return m_stores; }

const custom_store::Store *CustomStoreDatabase::findById(std::string_view id) const {
  const auto it = std::ranges::find(m_stores, id, &custom_store::Store::id);
  return it == m_stores.end() ? nullptr : &*it;
}

std::expected<custom_store::Store, std::string>
CustomStoreDatabase::addStore(std::string_view name, const custom_store::GitHubRepository &repository,
                              std::string_view branch) {
  if (containsRepository(repository, branch)) {
    return std::unexpected("This GitHub repository and branch are already configured");
  }

  const auto now = static_cast<std::uint64_t>(QDateTime::currentSecsSinceEpoch());
  custom_store::Store store{
      .id = std::format("cst-{}", QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString()),
      .name = std::string(name),
      .owner = repository.owner,
      .repository = repository.name,
      .branch = std::string(branch),
      .createdAt = now,
      .updatedAt = now,
  };

  m_stores.emplace_back(store);
  if (const auto result = saveStores(m_stores); !result) {
    m_stores.pop_back();
    return std::unexpected(result.error());
  }
  return store;
}

std::expected<custom_store::Store, std::string>
CustomStoreDatabase::updateStore(std::string_view id, std::string_view name,
                                 const custom_store::GitHubRepository &repository, std::string_view branch) {
  if (containsRepository(repository, branch, id)) {
    return std::unexpected("This GitHub repository and branch are already configured");
  }

  auto it = std::ranges::find(m_stores, id, &custom_store::Store::id);
  if (it == m_stores.end()) return std::unexpected("No custom store with that ID");

  auto previous = *it;
  it->name = name;
  it->owner = repository.owner;
  it->repository = repository.name;
  it->branch = branch;
  it->updatedAt = static_cast<std::uint64_t>(QDateTime::currentSecsSinceEpoch());

  if (const auto result = saveStores(m_stores); !result) {
    *it = previous;
    return std::unexpected(result.error());
  }
  return *it;
}

std::expected<custom_store::Store, std::string> CustomStoreDatabase::removeStore(std::string_view id) {
  auto it = std::ranges::find(m_stores, id, &custom_store::Store::id);
  if (it == m_stores.end()) return std::unexpected("No custom store with that ID");

  const auto index = static_cast<std::size_t>(std::distance(m_stores.begin(), it));
  auto removed = *it;
  m_stores.erase(it);

  if (const auto result = saveStores(m_stores); !result) {
    m_stores.insert(m_stores.begin() + static_cast<std::ptrdiff_t>(index), removed);
    return std::unexpected(result.error());
  }
  return removed;
}

std::expected<std::vector<custom_store::Store>, std::string> CustomStoreDatabase::loadStores() {
  std::vector<custom_store::Store> stores;
  if (const auto error = glz::read_file_json(stores, m_path.string(), m_buffer)) {
    return std::unexpected(glz::format_error(error));
  }
  return stores;
}

std::expected<void, std::string>
CustomStoreDatabase::saveStores(std::span<const custom_store::Store> stores) {
  if (const auto error = glz::write_file_json(stores, m_path.string(), m_buffer)) {
    return std::unexpected(std::format("Failed to save custom stores: {}", glz::format_error(error)));
  }
  return {};
}

bool CustomStoreDatabase::containsRepository(const custom_store::GitHubRepository &repository,
                                             std::string_view branch, std::string_view excludedId) const {
  return std::ranges::any_of(m_stores, [&](const custom_store::Store &store) {
    return store.id != excludedId && store.owner == repository.owner && store.repository == repository.name &&
           store.branch == branch;
  });
}