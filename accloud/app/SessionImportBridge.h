#pragma once

#include <QObject>
#include <QVariantMap>

namespace accloud {

class SessionImportBridge : public QObject {
  Q_OBJECT

 public:
  explicit SessionImportBridge(QObject* parent = nullptr);

  Q_INVOKABLE QString defaultSessionPath() const;
  Q_INVOKABLE QVariantMap importHar(const QString& harPath, const QString& sessionPath) const;

  // Vérifie que session.json existe et que la connexion cloud est valide.
  // Retourne : { sessionExists: bool, connectionOk: bool, message: string }
  Q_INVOKABLE QVariantMap checkStartup() const;
};

} // namespace accloud
