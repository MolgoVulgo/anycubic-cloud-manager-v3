#pragma once

#include <QObject>
#include <QString>
#include <QTranslator>

class QGuiApplication;
class QQmlApplicationEngine;

namespace accloud {

class AppI18nBridge : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString languageCode READ languageCode NOTIFY languageChanged)
  Q_PROPERTY(QString effectiveLanguageCode READ effectiveLanguageCode NOTIFY languageChanged)

 public:
  AppI18nBridge(QGuiApplication* app, QQmlApplicationEngine* engine, QObject* parent = nullptr);

  Q_INVOKABLE QString languageCode() const;
  Q_INVOKABLE QString effectiveLanguageCode() const;
  Q_INVOKABLE bool setLanguage(const QString& languageCode);
  Q_INVOKABLE void applyStartupLanguage();

 Q_SIGNALS:
  void languageChanged();

 private:
  QString normalizeRequestedLanguage(const QString& languageCode) const;
  QString detectSystemLanguage() const;
  bool applyLanguage(const QString& requestedLanguage);
  bool loadAppTranslatorFor(const QString& languageCode);
  bool loadQtTranslatorFor(const QString& languageCode);

  QGuiApplication* m_app{nullptr};
  QQmlApplicationEngine* m_engine{nullptr};
  QTranslator m_appTranslator;
  QTranslator m_qtTranslator;
  QString m_requestedLanguage{QStringLiteral("system")};
  QString m_effectiveLanguage{QStringLiteral("en")};
};

} // namespace accloud
