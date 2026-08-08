#pragma once

#include "custom-store/custom-store-service.hpp"
#include "qml/bridge-view.hpp"
#include <memory>
#include <optional>

class CustomStoreFormViewHost : public FormViewBase {
  Q_OBJECT

  Q_PROPERTY(QString name READ name WRITE setName NOTIFY formChanged)
  Q_PROPERTY(QString url READ url WRITE setUrl NOTIFY formChanged)
  Q_PROPERTY(QString branch READ branch WRITE setBranch NOTIFY formChanged)
  Q_PROPERTY(QString nameError READ nameError NOTIFY errorsChanged)
  Q_PROPERTY(QString urlError READ urlError NOTIFY errorsChanged)
  Q_PROPERTY(QString branchError READ branchError NOTIFY errorsChanged)

public:
  explicit CustomStoreFormViewHost(std::shared_ptr<CustomStoreService> service);
  CustomStoreFormViewHost(std::shared_ptr<CustomStoreService> service, custom_store::Store store);

  QUrl qmlComponentUrl() const override;
  QVariantMap qmlProperties() override;
  void initialize() override;

  Q_INVOKABLE void submit();

  QString name() const { return m_name; }
  QString url() const { return m_url; }
  QString branch() const { return m_branch; }
  QString nameError() const { return m_nameError; }
  QString urlError() const { return m_urlError; }
  QString branchError() const { return m_branchError; }

  void setName(const QString &value);
  void setUrl(const QString &value);
  void setBranch(const QString &value);

signals:
  void formChanged();
  void errorsChanged();

private:
  std::shared_ptr<CustomStoreService> m_service;
  std::optional<custom_store::Store> m_initialStore;
  QString m_name;
  QString m_url;
  QString m_branch = QStringLiteral("main");
  QString m_nameError;
  QString m_urlError;
  QString m_branchError;
};