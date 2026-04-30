#pragma once

#include <QObject>
#include <QVariantMap>

#include <cstddef>
#include <map>
#include <string>

namespace accloud {

class SessionImportBridge : public QObject {
  Q_OBJECT

 public:
  explicit SessionImportBridge(QObject* parent = nullptr);

  Q_INVOKABLE QString defaultSessionPath() const;
  Q_INVOKABLE QVariantMap importHar(const QString& harPath, const QString& sessionPath);
  Q_INVOKABLE QVariantMap analyzeHar(const QString& harPath, const QString& sessionPath);
  Q_INVOKABLE QVariantMap commitPendingSession(const QString& sessionPath);
  Q_INVOKABLE void discardPendingSession();
  Q_INVOKABLE QVariantMap sessionDetails(const QString& sessionPath = {}) const;

  // Vérifie que session.json existe et que la connexion cloud est valide.
  // Retourne : { sessionExists: bool, connectionOk: bool, message: string }
  Q_INVOKABLE QVariantMap checkStartup() const;

 private:
  bool m_hasPendingSession{false};
  std::map<std::string, std::string> m_pendingTokens;
  std::size_t m_pendingEntriesVisited{0};
  std::size_t m_pendingEntriesAccepted{0};
};

} // namespace accloud
