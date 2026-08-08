#include "custom-store/custom-store-installer.hpp"
#include "service-registry.hpp"
#include "services/extension-registry/extension-registry.hpp"
#include "services/local-storage/local-storage-service.hpp"
#include "services/oauth/oauth-service.hpp"
#include "generated/version.h"
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <format>

namespace fs = std::filesystem;

namespace {

QSet<QString> activeInstallIds;

std::expected<void, QString> normalizeSdkDependency(const fs::path &sourcePath) {
  const auto manifestPath = sourcePath / "package.json";
  QFile manifest(manifestPath);
  if (!manifest.open(QIODevice::ReadOnly)) {
    return std::unexpected(QObject::tr("Failed to read the staged extension manifest"));
  }

  QJsonParseError parseError;
  auto document = QJsonDocument::fromJson(manifest.readAll(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    return std::unexpected(QObject::tr("Failed to parse the staged extension manifest"));
  }

  auto root = document.object();
  auto dependencies = root.value(QStringLiteral("dependencies")).toObject();
  const auto dependency = dependencies.value(QStringLiteral("@vicinae/api")).toString();
  if (!dependency.startsWith(QStringLiteral("file:"))) return {};

  QString version = QStringLiteral(VICINAE_GIT_TAG);
  if (version.startsWith('v')) version.removeFirst();
  if (version.isEmpty()) version = QStringLiteral("latest");
  dependencies.insert(QStringLiteral("@vicinae/api"), version);
  root.insert(QStringLiteral("dependencies"), dependencies);

  QSaveFile output(manifestPath);
  if (!output.open(QIODevice::WriteOnly) || output.write(QJsonDocument(root).toJson()) < 0 ||
      !output.commit()) {
    return std::unexpected(QObject::tr("Failed to update the staged extension manifest"));
  }
  return {};
}

} // namespace

CustomStoreInstaller::CustomStoreInstaller(ExtensionRegistry &registry, custom_store::Store store,
                                           custom_store::CatalogEntry entry, QObject *parent)
    : QObject(parent), m_registry(registry), m_store(std::move(store)), m_entry(std::move(entry)) {
  connect(&m_process, &QProcess::readyReadStandardOutput, this,
          [this]() { m_processOutput.append(m_process.readAllStandardOutput()); });
  connect(&m_process, &QProcess::readyReadStandardError, this,
          [this]() { m_processOutput.append(m_process.readAllStandardError()); });
  connect(&m_process, &QProcess::finished, this, &CustomStoreInstaller::handleProcessFinished);
  connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart) fail(tr("Failed to start %1").arg(m_process.program()));
  });
}

void CustomStoreInstaller::start() {
  const auto id = installId();
  if (activeInstallIds.contains(id)) {
    fail(tr("An install is already running for this extension"));
    return;
  }
  activeInstallIds.insert(id);
  m_hasInstallLock = true;

  const auto npm = QStandardPaths::findExecutable(QStringLiteral("npm"));
  if (npm.isEmpty()) {
    fail(tr("npm must be available on PATH"));
    return;
  }

  m_workDirectory =
      std::make_unique<QTemporaryDir>(QDir::tempPath() + QStringLiteral("/vicinae-custom-store-XXXXXX"));
  if (!m_workDirectory->isValid()) {
    fail(tr("Failed to create a temporary build directory"));
    return;
  }

  m_sourcePath = fs::path(m_workDirectory->path().toStdString()) / "source";
  std::error_code error;
  fs::copy(m_entry.sourcePath, m_sourcePath, fs::copy_options::recursive, error);
  if (error) {
    fail(tr("Failed to stage extension source: %1").arg(QString::fromStdString(error.message())));
    return;
  }
  if (auto normalized = normalizeSdkDependency(m_sourcePath); !normalized) {
    fail(normalized.error());
    return;
  }

  m_stage = Stage::InstallingDependencies;
  emit stageChanged(tr("Installing dependencies..."));
  runProcess(npm, {QStringLiteral("install")}, m_sourcePath);
}

void CustomStoreInstaller::runProcess(const QString &program, const QStringList &arguments,
                                      const fs::path &cwd) {
  m_processOutput.clear();
  m_process.setWorkingDirectory(QString::fromStdString(cwd.string()));
  m_process.setProgram(program);
  m_process.setArguments(arguments);
  m_process.start();
}

void CustomStoreInstaller::handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
  m_processOutput.append(m_process.readAllStandardOutput());
  m_processOutput.append(m_process.readAllStandardError());

  if (m_stage == Stage::Finished) return;
  if (exitStatus != QProcess::NormalExit || exitCode != 0) {
    qWarning().noquote() << "Custom store build command failed:" << m_process.program()
                         << QString::fromUtf8(m_processOutput);
    fail(m_stage == Stage::InstallingDependencies ? tr("Failed to install extension dependencies")
                                                  : tr("Failed to build extension"));
    return;
  }

  if (m_stage == Stage::InstallingDependencies) {
    startBuild();
  } else if (m_stage == Stage::Building) {
    activateBuild();
  }
}

void CustomStoreInstaller::startBuild() {
  const auto extensionDirectory = ExtensionRegistry::localExtensionDirectory();
  std::error_code error;
  fs::create_directories(extensionDirectory, error);
  if (error) {
    fail(tr("Failed to create the extension directory"));
    return;
  }

  m_stagingDirectory = std::make_unique<QTemporaryDir>(
      QString::fromStdString((extensionDirectory / ".custom-store-install-XXXXXX").string()));
  if (!m_stagingDirectory->isValid()) {
    fail(tr("Failed to create an extension staging directory"));
    return;
  }

  m_buildPath = fs::path(m_stagingDirectory->path().toStdString()) / "bundle";
  m_stage = Stage::Building;
  emit stageChanged(tr("Building extension..."));

  const auto npm = QStandardPaths::findExecutable(QStringLiteral("npm"));
  runProcess(npm,
             {QStringLiteral("run"), QStringLiteral("build"), QStringLiteral("--"),
              QStringLiteral("--out"), QString::fromStdString(m_buildPath.string())},
             m_sourcePath);
}

void CustomStoreInstaller::activateBuild() {
  auto manifest = ExtensionManifest::fromPackageJson(m_buildPath);
  if (!manifest) {
    fail(tr("Built extension has an invalid manifest: %1").arg(manifest.error().m_message));
    return;
  }
  if (manifest->name != m_entry.manifest.name) {
    fail(tr("Built extension name does not match the store catalog"));
    return;
  }
  for (const auto &command : manifest->commands) {
    std::error_code error;
    if (!fs::is_regular_file(command.entrypoint, error)) {
      fail(tr("Built extension is missing command entrypoint %1").arg(command.name));
      return;
    }
  }

  const fs::path target = ExtensionRegistry::localExtensionDirectory() / installId().toStdString();
  const fs::path backup = target.parent_path() / std::format(".{}.backup", installId().toStdString());
  std::error_code error;
  fs::remove_all(backup, error);
  error.clear();

  const bool hadInstallation = fs::exists(target, error);
  if (error) {
    fail(tr("Failed to inspect the existing extension"));
    return;
  }
  if (hadInstallation) {
    fs::rename(target, backup, error);
    if (error) {
      fail(tr("Failed to stage the existing extension for replacement"));
      return;
    }
  }

  fs::rename(m_buildPath, target, error);
  if (error) {
    const auto activationError = error.message();
    if (hadInstallation) {
      error.clear();
      fs::rename(backup, target, error);
    }
    fail(tr("Failed to activate built extension: %1").arg(QString::fromStdString(activationError)));
    return;
  }

  fs::remove_all(backup, error);
  migrateLegacyInstallation();
  m_registry.requestScan();
  finish();
}

void CustomStoreInstaller::migrateLegacyInstallation() {
  const auto legacyId = legacyInstallId();
  const auto id = installId();
  if (legacyId == id) return;

  const auto legacyBundle = ExtensionRegistry::localExtensionDirectory() / legacyId.toStdString();
  std::error_code error;
  fs::remove_all(legacyBundle, error);
  if (error) qWarning() << "Failed to remove legacy custom store bundle" << legacyBundle.c_str();

  const auto legacySupport = ExtensionRegistry::supportDirectory(legacyId.toStdString());
  const auto support = ExtensionRegistry::supportDirectory(id.toStdString());
  if (fs::is_directory(legacySupport, error)) {
    error.clear();
    fs::create_directories(support, error);
    if (!error) {
      fs::copy(legacySupport, support, fs::copy_options::recursive | fs::copy_options::skip_existing,
               error);
    }
    if (!error) fs::remove_all(legacySupport, error);
    if (error) qWarning() << "Failed to migrate legacy custom store support data" << error.message();
  }

  auto *services = ServiceRegistry::instance();
  auto *storage = services->localStorage();
  const auto author = m_entry.manifest.author;
  const auto legacyProvider = QStringLiteral("@%1/%2").arg(author, legacyId);
  const auto provider = QStringLiteral("@%1/%2").arg(author, id);

  for (const auto &legacyNamespace : storage->namespaces()) {
    if (!legacyNamespace.startsWith(legacyProvider)) continue;

    const auto targetNamespace = provider + legacyNamespace.sliced(legacyProvider.size());
    const auto targetItems = storage->listNamespaceItems(targetNamespace);
    bool migrated = true;
    const auto legacyItems = storage->listNamespaceItems(legacyNamespace);
    for (auto it = legacyItems.begin(); it != legacyItems.end(); ++it) {
      if (!targetItems.contains(it.key()) && !storage->setItem(targetNamespace, it.key(), it.value())) {
        migrated = false;
      }
    }
    if (migrated) storage->clearNamespace(legacyNamespace);
  }

  auto &tokenStore = services->oauthService()->store();
  for (const auto &tokenSet : tokenStore.list()) {
    if (tokenSet.extensionId != legacyId) continue;

    if (!tokenStore.getTokenSet(id, tokenSet.providerId)) {
      OAuth::SetTokenSetPayload payload{
          .extensionId = id,
          .accessToken = tokenSet.accessToken,
          .providerId = tokenSet.providerId,
          .refreshToken = tokenSet.refreshToken,
          .idToken = tokenSet.idToken,
          .scope = tokenSet.scope,
          .expiresIn = tokenSet.expiresIn,
      };
      if (!tokenStore.setTokenSet(payload)) continue;
    }
    tokenStore.removeTokenSet(legacyId, tokenSet.providerId);
  }
}

void CustomStoreInstaller::fail(const QString &error) {
  if (m_stage == Stage::Finished) return;
  m_stage = Stage::Finished;
  if (m_hasInstallLock) activeInstallIds.remove(installId());
  emit completed(false, error);
  deleteLater();
}

void CustomStoreInstaller::finish() {
  m_stage = Stage::Finished;
  if (m_hasInstallLock) activeInstallIds.remove(installId());
  emit completed(true, {});
  deleteLater();
}

QString CustomStoreInstaller::installId() const {
  return QString::fromStdString(custom_store::installId(m_store, m_entry.manifest.name.toStdString()));
}

QString CustomStoreInstaller::legacyInstallId() const {
  return QStringLiteral("custom-store.%1.%2")
      .arg(QString::fromStdString(m_store.id), m_entry.manifest.name);
}