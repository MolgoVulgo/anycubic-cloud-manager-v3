#include "SessionImportBridge.h"

#include "infra/cloud/CloudClient.h"
#include "infra/cloud/HarImporter.h"
#include "infra/logging/JsonlLogger.h"

#include <QDir>
#include <QStringList>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace accloud {
namespace {

std::filesystem::path toFsPath(const QString& rawPath) {
  QString normalized = rawPath.trimmed();
  if (normalized.startsWith("~/")) {
    normalized = QDir::homePath() + normalized.mid(1);
  }
  return std::filesystem::path(normalized.toStdString());
}

QStringList tokenKeyList(const std::map<std::string, std::string>& tokens) {
  QStringList keys;
  keys.reserve(static_cast<qsizetype>(tokens.size()));
  for (const auto& [key, _] : tokens) {
    keys.push_back(QString::fromStdString(key));
  }
  return keys;
}

} // namespace

SessionImportBridge::SessionImportBridge(QObject* parent) : QObject(parent) {}

QString SessionImportBridge::defaultSessionPath() const {
  const auto path = cloud::resolveSessionPath();
  logging::debug("app", "session_import_bridge", "default_session_path",
                 "Resolved default session path", {{"path", path.string()}});
  return QString::fromStdString(path.string());
}

QVariantMap SessionImportBridge::importHar(const QString& harPath,
                                           const QString& sessionPath) const {
  QVariantMap out;

  const QString trimmedHar = harPath.trimmed();
  if (trimmedHar.isEmpty()) {
    logging::warn("app", "session_import_bridge", "import_rejected",
                  "HAR import rejected because path is empty");
    out.insert("ok", false);
    out.insert("message", "HAR file path is required");
    return out;
  }

  cloud::HarImportOptions options;
  const QString trimmedSession = sessionPath.trimmed();
  if (!trimmedSession.isEmpty()) {
    options.sessionPathOverride = toFsPath(trimmedSession);
  }

  logging::info("app", "session_import_bridge", "import_started", "QML requested HAR import",
                {{"har_path", toFsPath(trimmedHar).string()},
                 {"session_path", trimmedSession.isEmpty() ? "<default>" : toFsPath(trimmedSession).string()}});

  const cloud::HarImportResult result = cloud::importHarFile(toFsPath(trimmedHar), options);
  if (result.ok) {
    logging::info("app", "session_import_bridge", "import_succeeded",
                  "HAR import succeeded from QML",
                  {{"entries_visited", std::to_string(result.entriesVisited)},
                   {"entries_accepted", std::to_string(result.entriesAccepted)},
                   {"token_count", std::to_string(result.session.tokens.size())}});
  } else {
    logging::error("app", "session_import_bridge", "import_failed",
                   "HAR import failed from QML", {{"reason", result.message}});
  }

  out.insert("ok", result.ok);
  out.insert("message", QString::fromStdString(result.message));
  out.insert("entriesVisited", static_cast<qulonglong>(result.entriesVisited));
  out.insert("entriesAccepted", static_cast<qulonglong>(result.entriesAccepted));
  out.insert("tokenKeys", tokenKeyList(result.session.tokens));
  return out;
}

QVariantMap SessionImportBridge::checkStartup() const {
    QVariantMap out;

    // 1. Vérifier que session.json existe et est lisible
    const cloud::LoadSessionResult loaded = cloud::loadSessionFile();
    if (!loaded.ok) {
        logging::info("app", "session_import_bridge", "startup_no_session",
                      "Session introuvable ou invalide",
                      {{"path", loaded.path.string()}});
        out.insert("sessionExists", false);
        out.insert("connectionOk", false);
        out.insert("message",
                   QString("Aucun fichier session.json trouvé. "
                           "Importez un fichier HAR pour créer une session."));
        return out;
    }
    out.insert("sessionExists", true);

    // 2. Récupérer le token d'accès
    const auto accIt = loaded.session.tokens.find("access_token");
    if (accIt == loaded.session.tokens.end()) {
        out.insert("connectionOk", false);
        out.insert("message", QString("session.json ne contient pas de access_token."));
        return out;
    }
    const auto tokIt = loaded.session.tokens.find("token");
    const std::string xxToken =
        (tokIt != loaded.session.tokens.end()) ? tokIt->second : std::string{};

    // 3. Tester la connexion cloud
    logging::info("app", "session_import_bridge", "startup_checking_cloud",
                  "Vérification de la connexion cloud au démarrage");
    const cloud::CloudCheckResult check = cloud::checkCloudAuth(accIt->second, xxToken);

    out.insert("connectionOk", check.ok);
    out.insert("message", QString::fromStdString(check.message));

    if (check.ok) {
        logging::info("app", "session_import_bridge", "startup_ok",
                      "Démarrage validé : session valide et cloud accessible");
    } else {
        logging::warn("app", "session_import_bridge", "startup_cloud_failed",
                      "Cloud inaccessible au démarrage", {{"reason", check.message}});
    }
    return out;
}

} // namespace accloud
