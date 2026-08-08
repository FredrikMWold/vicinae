#include "custom-store/custom-store-service.hpp"
#include "internal/zip/unzip.hpp"
#include <QFutureWatcher>
#include <QPromise>
#include <QTemporaryDir>
#include <QtConcurrent/QtConcurrentRun>
#include <algorithm>

namespace fs = std::filesystem;

namespace {

CustomStoreService::CatalogResult scanSnapshot(const fs::path &path) {
  std::error_code error;
  std::vector<custom_store::CatalogEntry> entries;

  for (const auto &child : fs::directory_iterator(path, error)) {
    if (error) return std::unexpected(std::format("Failed to read snapshot: {}", error.message()));
    if (!child.is_directory(error) || child.path().filename().string().starts_with('.')) continue;
    if (!fs::is_regular_file(child.path() / "package.json", error)) continue;

    auto manifest = ExtensionManifest::fromPackageJson(child.path());
    if (!manifest) {
      qWarning() << "Skipping custom store extension at" << child.path().c_str()
                 << manifest.error().m_message;
      continue;
    }

    entries.emplace_back(custom_store::CatalogEntry{
        .folder = child.path().filename().string(),
        .sourcePath = child.path(),
        .manifest = std::move(*manifest),
    });
  }

  if (error) return std::unexpected(std::format("Failed to read snapshot: {}", error.message()));

  std::ranges::sort(entries, {}, [](const custom_store::CatalogEntry &entry) {
    return entry.manifest.title.toCaseFolded();
  });
  return entries;
}

CustomStoreService::CatalogResult
replaceSnapshot(const fs::path &cacheDirectory, const custom_store::Store &store, const QByteArray &archive) {
  std::error_code error;
  fs::create_directories(cacheDirectory, error);
  if (error) return std::unexpected(std::format("Failed to create cache: {}", error.message()));

  QTemporaryDir staging(QString::fromStdString((cacheDirectory / ".refresh-XXXXXX").string()));
  if (!staging.isValid()) return std::unexpected("Failed to create temporary store snapshot");

  Unzipper unzip(std::string_view(archive.constData(), static_cast<std::size_t>(archive.size())));
  if (!unzip) return std::unexpected("GitHub returned an invalid ZIP archive");

  const fs::path stagingPath = staging.path().toStdString();
  unzip.extract(stagingPath, {.stripComponents = 1});

  auto stagedCatalog = scanSnapshot(stagingPath);
  if (!stagedCatalog) return std::unexpected(stagedCatalog.error());
  if (stagedCatalog->empty()) {
    return std::unexpected("No top-level extension folders with valid package.json files were found");
  }

  const fs::path target = cacheDirectory / store.id;
  const fs::path backup = cacheDirectory / std::format(".{}.backup", store.id);
  fs::remove_all(backup, error);
  error.clear();

  const bool hadSnapshot = fs::exists(target, error);
  if (error) return std::unexpected(std::format("Failed to inspect existing snapshot: {}", error.message()));
  if (hadSnapshot) {
    fs::rename(target, backup, error);
    if (error) return std::unexpected(std::format("Failed to stage existing snapshot: {}", error.message()));
  }

  staging.setAutoRemove(false);
  fs::rename(stagingPath, target, error);
  if (error) {
    const auto renameError = error.message();
    fs::remove_all(stagingPath, error);
    if (hadSnapshot) {
      error.clear();
      fs::rename(backup, target, error);
    }
    return std::unexpected(std::format("Failed to activate refreshed snapshot: {}", renameError));
  }

  fs::remove_all(backup, error);
  return scanSnapshot(target);
}

} // namespace

CustomStoreService::CustomStoreService(const std::filesystem::path &dataDirectory, QObject *parent)
    : QObject(parent), m_database(dataDirectory / "stores.json"), m_cacheDirectory(dataDirectory / "cache") {}

const std::vector<custom_store::Store> &CustomStoreService::stores() const { return m_database.stores(); }

const custom_store::Store *CustomStoreService::findById(std::string_view id) const {
  return m_database.findById(id);
}

std::expected<custom_store::Store, std::string>
CustomStoreService::createStore(const QString &name, const QString &url, const QString &branch) {
  const auto trimmedName = name.trimmed();
  const auto trimmedBranch = branch.trimmed();
  if (trimmedName.isEmpty()) return std::unexpected("Store name is required");
  if (trimmedBranch.isEmpty()) return std::unexpected("Branch is required");

  auto repository = custom_store::parseGitHubRepository(url.trimmed().toStdString());
  if (!repository) return std::unexpected(repository.error());

  auto result = m_database.addStore(trimmedName.toStdString(), *repository, trimmedBranch.toStdString());
  if (result) emit storesChanged();
  return result;
}

std::expected<custom_store::Store, std::string> CustomStoreService::updateStore(const QString &id,
                                                                                const QString &name,
                                                                                const QString &url,
                                                                                const QString &branch) {
  const auto trimmedName = name.trimmed();
  const auto trimmedBranch = branch.trimmed();
  if (trimmedName.isEmpty()) return std::unexpected("Store name is required");
  if (trimmedBranch.isEmpty()) return std::unexpected("Branch is required");

  auto repository = custom_store::parseGitHubRepository(url.trimmed().toStdString());
  if (!repository) return std::unexpected(repository.error());

  const auto previous = m_database.findById(id.toStdString());
  const bool sourceChanged =
      previous && (previous->owner != repository->owner || previous->repository != repository->name ||
                   previous->branch != trimmedBranch.toStdString());
  auto result = m_database.updateStore(id.toStdString(), trimmedName.toStdString(), *repository,
                                       trimmedBranch.toStdString());
  if (result) {
    if (sourceChanged) {
      std::error_code error;
      std::filesystem::remove_all(snapshotPath(*result), error);
    }
    emit storesChanged();
  }
  return result;
}

std::expected<custom_store::Store, std::string> CustomStoreService::removeStore(const QString &id) {
  auto result = m_database.removeStore(id.toStdString());
  if (result) {
    std::error_code error;
    std::filesystem::remove_all(m_cacheDirectory / result->id, error);
    emit storesChanged();
  }
  return result;
}

fs::path CustomStoreService::snapshotPath(const custom_store::Store &store) const {
  return m_cacheDirectory / store.id;
}

bool CustomStoreService::hasSnapshot(const custom_store::Store &store) const {
  std::error_code error;
  return fs::is_directory(snapshotPath(store), error);
}

CustomStoreService::CatalogResult CustomStoreService::catalog(const custom_store::Store &store) const {
  if (!hasSnapshot(store)) return std::unexpected("Store has not been refreshed yet");
  return scanSnapshot(snapshotPath(store));
}

QFuture<CustomStoreService::CatalogResult>
CustomStoreService::refreshStore(const custom_store::Store &store) {
  auto promise = std::make_shared<QPromise<CatalogResult>>();
  promise->start();
  auto future = promise->future();

  const auto branch = QUrl::toPercentEncoding(QString::fromStdString(store.branch));
  const auto url = QStringLiteral("https://codeload.github.com/%1/%2/zip/refs/heads/%3")
                       .arg(QString::fromStdString(store.owner), QString::fromStdString(store.repository),
                            QString::fromUtf8(branch));
  auto rawWatcher = new QFutureWatcher<http::Client::Result<QByteArray>>(this);

  connect(rawWatcher, &QFutureWatcher<http::Client::Result<QByteArray>>::finished, this,
          [this, rawWatcher, promise, store]() {
            auto result = rawWatcher->result();
            rawWatcher->deleteLater();
            if (!result) {
              promise->addResult(std::unexpected(result.error()));
              promise->finish();
              return;
            }

            auto worker = QtConcurrent::run(replaceSnapshot, m_cacheDirectory, store, *result);
            auto workerWatcher = new QFutureWatcher<CatalogResult>(this);
            connect(workerWatcher, &QFutureWatcher<CatalogResult>::finished, this,
                    [this, workerWatcher, promise, store]() {
                      auto workerResult = workerWatcher->result();
                      promise->addResult(workerResult);
                      promise->finish();
                      if (workerResult) emit snapshotChanged(QString::fromStdString(store.id));
                      workerWatcher->deleteLater();
                    });
            workerWatcher->setFuture(worker);
          });
  rawWatcher->setFuture(m_http.getRaw(url));
  return future;
}