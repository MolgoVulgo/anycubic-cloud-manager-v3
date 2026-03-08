#include "AppI18nBridge.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QSettings>

namespace accloud {

namespace {
constexpr const char* kLanguageKey = "ui.language";
}

AppI18nBridge::AppI18nBridge(QGuiApplication* app, QQmlApplicationEngine* engine, QObject* parent)
    : QObject(parent), m_app(app), m_engine(engine) {}

QString AppI18nBridge::languageCode() const {
  return m_requestedLanguage;
}

QString AppI18nBridge::effectiveLanguageCode() const {
  return m_effectiveLanguage;
}

QString AppI18nBridge::normalizeRequestedLanguage(const QString& languageCode) const {
  const QString normalized = languageCode.trimmed().toLower();
  if (normalized.isEmpty() || normalized == QStringLiteral("system")) {
    return QStringLiteral("system");
  }
  if (normalized.startsWith(QStringLiteral("fr"))) {
    return QStringLiteral("fr");
  }
  if (normalized.startsWith(QStringLiteral("en"))) {
    return QStringLiteral("en");
  }
  return QStringLiteral("en");
}

QString AppI18nBridge::detectSystemLanguage() const {
  const QString systemLocale = QLocale::system().name().toLower();
  if (systemLocale.startsWith(QStringLiteral("fr"))) {
    return QStringLiteral("fr");
  }
  if (systemLocale.startsWith(QStringLiteral("en"))) {
    return QStringLiteral("en");
  }
  return QStringLiteral("en");
}

bool AppI18nBridge::loadAppTranslatorFor(const QString& languageCode) {
  return m_appTranslator.load(QLocale(languageCode), QStringLiteral("accloud"),
                              QStringLiteral("_"), QStringLiteral(":/i18n"));
}

bool AppI18nBridge::loadQtTranslatorFor(const QString& languageCode) {
  const QString qtTranslationsPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
  return m_qtTranslator.load(QLocale(languageCode), QStringLiteral("qtbase"),
                             QStringLiteral("_"), qtTranslationsPath);
}

bool AppI18nBridge::applyLanguage(const QString& requestedLanguage) {
  if (m_app == nullptr) {
    return false;
  }

  QString requested = normalizeRequestedLanguage(requestedLanguage);
  QString effective = (requested == QStringLiteral("system")) ? detectSystemLanguage() : requested;
  if (effective != QStringLiteral("en") && effective != QStringLiteral("fr")) {
    effective = QStringLiteral("en");
  }

  m_app->removeTranslator(&m_appTranslator);
  m_app->removeTranslator(&m_qtTranslator);

  bool appLoaded = loadAppTranslatorFor(effective);
  if (!appLoaded && effective != QStringLiteral("en")) {
    effective = QStringLiteral("en");
    appLoaded = loadAppTranslatorFor(effective);
  }
  if (appLoaded) {
    m_app->installTranslator(&m_appTranslator);
  }

  if (loadQtTranslatorFor(effective)) {
    m_app->installTranslator(&m_qtTranslator);
  }

  const bool changed =
      (m_requestedLanguage != requested) || (m_effectiveLanguage != effective);
  m_requestedLanguage = requested;
  m_effectiveLanguage = effective;

  if (m_engine != nullptr) {
    m_engine->retranslate();
  }
  if (changed) {
    emit languageChanged();
  }
  return true;
}

bool AppI18nBridge::setLanguage(const QString& languageCode) {
  const QString requested = normalizeRequestedLanguage(languageCode);
  const QString languageKey = QString::fromLatin1(kLanguageKey);

  QString org = QCoreApplication::organizationName();
  if (org.trimmed().isEmpty()) {
    org = QStringLiteral("accloud");
  }
  QString app = QCoreApplication::applicationName();
  if (app.trimmed().isEmpty()) {
    app = QStringLiteral("accloud");
  }
  QSettings settings(org, app);
  settings.setValue(languageKey, requested);
  settings.sync();

  return applyLanguage(requested);
}

void AppI18nBridge::applyStartupLanguage() {
  const QString languageKey = QString::fromLatin1(kLanguageKey);
  QString org = QCoreApplication::organizationName();
  if (org.trimmed().isEmpty()) {
    org = QStringLiteral("accloud");
  }
  QString app = QCoreApplication::applicationName();
  if (app.trimmed().isEmpty()) {
    app = QStringLiteral("accloud");
  }
  QSettings settings(org, app);
  const QString requested =
      normalizeRequestedLanguage(settings.value(languageKey, QStringLiteral("system")).toString());
  applyLanguage(requested);
}

} // namespace accloud
