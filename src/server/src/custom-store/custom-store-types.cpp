#include "custom-store/custom-store-types.hpp"
#include <QUrl>
#include <QStringList>
#include <algorithm>
#include <cctype>
#include <format>

namespace custom_store {

namespace {

bool isValidComponent(std::string_view value) {
  return !value.empty() && std::ranges::all_of(value, [](unsigned char c) {
    return std::isalnum(c) != 0 || c == '-' || c == '_' || c == '.';
  });
}

} // namespace

std::string GitHubRepository::canonicalUrl() const {
  return std::format("https://github.com/{}/{}", owner, name);
}

std::string Store::canonicalUrl() const {
  return GitHubRepository{.owner = owner, .name = repository}.canonicalUrl();
}

std::expected<GitHubRepository, std::string> parseGitHubRepository(std::string_view url) {
  const QUrl parsed(QString::fromUtf8(url.data(), static_cast<qsizetype>(url.size())));

  if (!parsed.isValid() || parsed.scheme() != QStringLiteral("https") ||
      parsed.host().compare(QStringLiteral("github.com"), Qt::CaseInsensitive) != 0 || parsed.hasQuery() ||
      parsed.hasFragment()) {
    return std::unexpected("Expected an https://github.com/<owner>/<repository> URL");
  }

  const auto parts = parsed.path().split('/', Qt::SkipEmptyParts);
  if (parts.size() != 2) {
    return std::unexpected("Expected an https://github.com/<owner>/<repository> URL");
  }

  auto owner = parts.front().toStdString();
  auto repository = parts.back().toStdString();
  if (repository.ends_with(".git")) repository.resize(repository.size() - 4);

  if (!isValidComponent(owner) || !isValidComponent(repository)) {
    return std::unexpected("GitHub owner and repository names contain unsupported characters");
  }

  return GitHubRepository{.owner = std::move(owner), .name = std::move(repository)};
}

std::string installId(const Store &, std::string_view extensionName) {
  return std::string(extensionName);
}

} // namespace custom_store