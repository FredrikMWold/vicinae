#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <filesystem>
#include "services/extension-registry/extension-manifest.hpp"

namespace custom_store {

struct GitHubRepository {
  std::string owner;
  std::string name;

  std::string canonicalUrl() const;
};

struct Store {
  std::string id;
  std::string name;
  std::string owner;
  std::string repository;
  std::string branch;
  std::uint64_t createdAt = 0;
  std::uint64_t updatedAt = 0;

  std::string canonicalUrl() const;
};

struct CatalogEntry {
  std::string folder;
  std::filesystem::path sourcePath;
  ExtensionManifest manifest;
};

std::expected<GitHubRepository, std::string> parseGitHubRepository(std::string_view url);
std::string installId(const Store &store, std::string_view extensionName);

} // namespace custom_store