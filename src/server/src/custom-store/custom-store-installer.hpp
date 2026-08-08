#pragma once

#include "custom-store/custom-store-types.hpp"
#include <QObject>
#include <QProcess>
#include <QTemporaryDir>
#include <memory>

class ExtensionRegistry;

class CustomStoreInstaller : public QObject {
  Q_OBJECT

signals:
  void stageChanged(const QString &message);
  void completed(bool success, const QString &error);

public:
  CustomStoreInstaller(ExtensionRegistry &registry, custom_store::Store store,
                       custom_store::CatalogEntry entry, QObject *parent = nullptr);

  void start();

private:
  enum class Stage { Idle, InstallingDependencies, Building, Finished };

  void runProcess(const QString &program, const QStringList &arguments, const std::filesystem::path &cwd);
  void handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
  void startBuild();
  void activateBuild();
  void migrateLegacyInstallation();
  void fail(const QString &error);
  void finish();
  QString installId() const;
  QString legacyInstallId() const;

  ExtensionRegistry &m_registry;
  custom_store::Store m_store;
  custom_store::CatalogEntry m_entry;
  QProcess m_process;
  Stage m_stage = Stage::Idle;
  QByteArray m_processOutput;
  std::unique_ptr<QTemporaryDir> m_workDirectory;
  std::unique_ptr<QTemporaryDir> m_stagingDirectory;
  std::filesystem::path m_sourcePath;
  std::filesystem::path m_buildPath;
  bool m_hasInstallLock = false;
};