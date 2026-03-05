#include "UiSettingsBridge.h"

#include <QCoreApplication>
#include <QSettings>

namespace accloud {

UiSettingsBridge::UiSettingsBridge(QObject* parent)
    : QObject(parent) {
  QString org = QCoreApplication::organizationName();
  if (org.trimmed().isEmpty()) {
    org = QStringLiteral("accloud");
  }

  QString app = QCoreApplication::applicationName();
  if (app.trimmed().isEmpty()) {
    app = QStringLiteral("accloud");
  }

  m_settings = new QSettings(org, app, this);
}

UiSettingsBridge::~UiSettingsBridge() = default;

QString UiSettingsBridge::getString(const QString& key, const QString& defaultValue) const {
  if (!m_settings) return defaultValue;
  return m_settings->value(key, defaultValue).toString();
}

void UiSettingsBridge::setString(const QString& key, const QString& value) {
  if (!m_settings) return;
  m_settings->setValue(key, value);
}

bool UiSettingsBridge::hasKey(const QString& key) const {
  if (!m_settings) return false;
  return m_settings->contains(key);
}

void UiSettingsBridge::removeKey(const QString& key) {
  if (!m_settings) return;
  m_settings->remove(key);
}

void UiSettingsBridge::sync() {
  if (!m_settings) return;
  m_settings->sync();
}

} // namespace accloud
