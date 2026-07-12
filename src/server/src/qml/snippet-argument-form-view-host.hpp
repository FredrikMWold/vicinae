#pragma once
#include "bridge-view.hpp"
#include "services/snippet/snippet-service.hpp"
#include <QVariantList>

class SnippetArgumentFormViewHost : public FormViewBase {
  Q_OBJECT

  Q_PROPERTY(QVariantList arguments READ arguments NOTIFY argumentsChanged)

public:
  explicit SnippetArgumentFormViewHost(SnippetArgumentExpansionRequest request);

  QUrl qmlComponentUrl() const override;
  QVariantMap qmlProperties() override;
  void initialize() override;

  Q_INVOKABLE void setArgumentValue(int index, const QString &value);
  Q_INVOKABLE void submit();

  QVariantList arguments() const;

signals:
  void argumentsChanged();

private:
  struct ArgumentField {
    QString name;
    QString value;
    bool required = false;
    QString error;
  };

  bool validate();

  SnippetArgumentExpansionRequest m_request;
  SnippetService *m_service = nullptr;
  std::vector<ArgumentField> m_arguments;
};
