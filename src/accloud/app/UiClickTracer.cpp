#include "UiClickTracer.h"

#include "infra/logging/JsonlLogger.h"

#include <QEvent>
#include <QMetaObject>
#include <QMouseEvent>
#include <QObject>
#include <QQuickItem>
#include <QVariant>

namespace accloud {

UiClickTracer::UiClickTracer(QObject* parent)
    : QObject(parent) {}

bool UiClickTracer::eventFilter(QObject* watched, QEvent* event) {
  if (watched == nullptr || event == nullptr) {
    return QObject::eventFilter(watched, event);
  }

  const bool isUiObject =
      watched->inherits("QQuickItem") || watched->inherits("QQuickAbstractButton")
      || watched->inherits("QQuickWindow");
  if (!isUiObject) {
    return QObject::eventFilter(watched, event);
  }

  if (event->type() != QEvent::MouseButtonRelease) {
    return QObject::eventFilter(watched, event);
  }

  if (watched->inherits("QQuickWindow")) {
    return QObject::eventFilter(watched, event);
  }

  const auto* mouseEvent = static_cast<QMouseEvent*>(event);
  logging::FieldMap fields;
  fields["class"] = watched->metaObject()->className();

  const QString objectName = watched->objectName().trimmed();
  if (!objectName.isEmpty()) {
    fields["object_name"] = objectName.toStdString();
  }

  const QVariant text = watched->property("text");
  if (text.isValid()) {
    const QString value = text.toString().trimmed();
    if (!value.isEmpty()) {
      fields["text"] = value.left(120).toStdString();
    }
  }

  const QVariant title = watched->property("title");
  if (title.isValid()) {
    const QString value = title.toString().trimmed();
    if (!value.isEmpty()) {
      fields["title"] = value.left(120).toStdString();
    }
  }

  if (const auto* item = qobject_cast<const QQuickItem*>(watched); item != nullptr) {
    fields["enabled"] = item->isEnabled() ? "true" : "false";
    fields["visible"] = item->isVisible() ? "true" : "false";
  }

  fields["button"] = std::to_string(static_cast<int>(mouseEvent->button()));
  fields["x"] = std::to_string(static_cast<long long>(mouseEvent->position().x()));
  fields["y"] = std::to_string(static_cast<long long>(mouseEvent->position().y()));

  logging::debug("app", "ui_click_tracer", "click_action",
                 "UI click action captured",
                 std::move(fields));

  return QObject::eventFilter(watched, event);
}

} // namespace accloud
