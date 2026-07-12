#include "snippet-argument-form-view-host.hpp"
#include "navigation-controller.hpp"
#include "service-registry.hpp"
#include "services/toast/toast-service.hpp"
#include "ui/action-pannel/action.hpp"
#include <QTimer>
#include <QUrl>

namespace {
constexpr int SUBMIT_INJECTION_DELAY_MS = 150;
}

SnippetArgumentFormViewHost::SnippetArgumentFormViewHost(SnippetArgumentExpansionRequest request)
    : FormViewBase(), m_request(std::move(request)) {
  m_arguments.reserve(m_request.arguments.size());
  for (const auto &argument : m_request.arguments) {
    m_arguments.emplace_back(ArgumentField{
        .name = argument.name,
        .value = argument.defaultValue,
        .required = argument.defaultValue.isEmpty(),
    });
  }
}

QUrl SnippetArgumentFormViewHost::qmlComponentUrl() const {
  return QUrl(QStringLiteral("qrc:/Vicinae/SnippetArgumentFormView.qml"));
}

QVariantMap SnippetArgumentFormViewHost::qmlProperties() {
  return {{QStringLiteral("host"), QVariant::fromValue(this)}};
}

void SnippetArgumentFormViewHost::initialize() {
  BaseView::initialize();

  m_service = context()->services->snippetService();
  setNavigationTitle(QStringLiteral("Expand \"%1\"").arg(QString::fromStdString(m_request.snippet.name)));

  auto panel = std::make_unique<FormActionPanelState>();
  auto section = panel->createSection();
  auto submitAction = new StaticAction(QStringLiteral("Insert Snippet"), ImageURL::builtin(BuiltinIcon::EnterKey),
                                       [this]() { submit(); });
  section->addAction(submitAction);
  setActions(std::move(panel));
}

QVariantList SnippetArgumentFormViewHost::arguments() const {
  QVariantList list;
  list.reserve(static_cast<qsizetype>(m_arguments.size()));

  for (const auto &argument : m_arguments) {
    QVariantMap item;
    const auto label = argument.name.isEmpty() ? QStringLiteral("Argument") : argument.name;
    item[QStringLiteral("name")] = argument.name;
    item[QStringLiteral("label")] = label;
    item[QStringLiteral("placeholder")] = label;
    item[QStringLiteral("value")] = argument.value;
    item[QStringLiteral("required")] = argument.required;
    item[QStringLiteral("error")] = argument.error;
    list.append(item);
  }

  return list;
}

void SnippetArgumentFormViewHost::setArgumentValue(int index, const QString &value) {
  if (index < 0 || static_cast<size_t>(index) >= m_arguments.size()) return;

  auto &argument = m_arguments[static_cast<size_t>(index)];
  argument.value = value;

  if (!argument.error.isEmpty() && (!argument.required || !argument.value.isEmpty())) {
    argument.error.clear();
    emit argumentsChanged();
  }
}

bool SnippetArgumentFormViewHost::validate() {
  bool valid = true;

  for (auto &argument : m_arguments) {
    argument.error.clear();
    if (argument.required && argument.value.isEmpty()) {
      argument.error = QStringLiteral("Required");
      valid = false;
    }
  }

  emit argumentsChanged();
  return valid;
}

void SnippetArgumentFormViewHost::submit() {
  if (!m_service) return;

  if (!validate()) {
    context()->services->toastService()->failure("Validation failed");
    return;
  }

  std::vector<std::pair<QString, QString>> values;
  values.reserve(m_arguments.size());
  for (const auto &argument : m_arguments) {
    values.emplace_back(argument.name, argument.value);
  }

  context()->navigation->closeWindow({.popToRootType = PopToRootType::Immediate, .clearRootSearch = true});

  QTimer::singleShot(SUBMIT_INJECTION_DELAY_MS, m_service,
                     [service = m_service, request = m_request, values]() {
                       service->completeArgumentExpansion(request, values);
                     });
}
