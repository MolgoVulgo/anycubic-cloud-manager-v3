#include "SessionImportBridge.h"

#include "infra/cloud/CloudClient.h"
#include "infra/cloud/HarImporter.h"
#include "infra/logging/JsonlLogger.h"

#include <QDir>
#include <QStringList>

#include <algorithm>
#include <filesystem>
#include <map>
#include <optional>
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

std::optional<std::filesystem::path> optionalFsPath(const QString& rawPath) {
  if (rawPath.trimmed().isEmpty()) {
    return std::nullopt;
  }
  return toFsPath(rawPath);
}

QStringList tokenKeyList(const std::map<std::string, std::string>& tokens) {
  QStringList keys;
  keys.reserve(static_cast<qsizetype>(tokens.size()));
  for (const auto& [key, _] : tokens) {
    keys.push_back(QString::fromStdString(key));
  }
  keys.sort(Qt::CaseInsensitive);
  return keys;
}

QString truncateSecretValue(const std::string& value) {
  if (value.empty()) {
    return QStringLiteral("<empty>");
  }
  if (value.size() <= 8) {
    return QStringLiteral("<redacted> (len=%1)").arg(value.size());
  }
  const QString head = QString::fromStdString(value.substr(0, 4));
  const QString tail = QString::fromStdString(value.substr(value.size() - 3));
  return QStringLiteral("%1...%2 (len=%3)").arg(head, tail).arg(value.size());
}

std::string pickXxToken(const std::map<std::string, std::string>& tokens) {
  const auto tokIt = tokens.find("token");
  if (tokIt != tokens.end()) {
    return tokIt->second;
  }
  return {};
}

void finalizeUiMessage(QVariantMap& out) {
  if (!out.contains("message")) {
    return;
  }
  const QString message = out.value("message").toString().trimmed();
  const QString lowered = message.toLower();
  const bool ok = out.value("ok").toBool();

  QString key = ok ? QStringLiteral("info.ok") : QStringLiteral("error.generic");
  if (lowered.contains("har")) {
    key = ok ? QStringLiteral("info.session.har") : QStringLiteral("error.session.har");
  } else if (lowered.contains("session")) {
    key = ok ? QStringLiteral("info.session") : QStringLiteral("error.session");
  } else if (lowered.contains("token")) {
    key = QStringLiteral("error.session.token");
  } else if (lowered.contains("cloud")) {
    key = ok ? QStringLiteral("info.cloud") : QStringLiteral("error.cloud");
  }

  out.insert("messageKey", key);
}

} // namespace

SessionImportBridge::SessionImportBridge(QObject* parent) : QObject(parent) {}

QString SessionImportBridge::defaultSessionPath() const {
  const auto path = cloud::resolveSessionPath();
  logging::debug("app", "session_import_bridge", "default_session_path",
                 "Resolved default session path", {{"path", path.string()}});
  return QString::fromStdString(path.string());
}

QVariantMap SessionImportBridge::importHar(const QString& harPath, const QString& sessionPath) {
  QVariantMap analyzed = analyzeHar(harPath, sessionPath);
  if (analyzed.value("ok").toBool()) {
    return commitPendingSession(sessionPath);
  }
  return analyzed;
}

QVariantMap SessionImportBridge::analyzeHar(const QString& harPath, const QString& sessionPath) {
  QVariantMap out;

  const QString trimmedHar = harPath.trimmed();
  if (trimmedHar.isEmpty()) {
    logging::warn("app", "session_import_bridge", "analyze_rejected",
                  "HAR analyze rejected because path is empty");
    out.insert("ok", false);
    out.insert("message", "HAR file path is required");
    out.insert("pendingValid", false);
    finalizeUiMessage(out);
    return out;
  }

  cloud::HarImportOptions options;
  options.persistSession = false;
  options.mergeWithExistingSession = true;
  const std::optional<std::filesystem::path> pathOverride = optionalFsPath(sessionPath);
  if (pathOverride.has_value()) {
    options.sessionPathOverride = *pathOverride;
  }

  logging::info("app", "session_import_bridge", "analyze_started",
                "QML requested HAR analysis",
                {{"har_path", toFsPath(trimmedHar).string()},
                 {"session_path", pathOverride.has_value() ? pathOverride->string() : "<default>"}});

  const cloud::HarImportResult result = cloud::importHarFile(toFsPath(trimmedHar), options);
  if (result.ok) {
    m_hasPendingSession = true;
    m_pendingTokens = result.session.tokens;
    m_pendingEntriesVisited = result.entriesVisited;
    m_pendingEntriesAccepted = result.entriesAccepted;
    m_pendingMessage = result.message;
    logging::info("app", "session_import_bridge", "analyze_succeeded",
                  "HAR analysis succeeded from QML",
                  {{"entries_visited", std::to_string(result.entriesVisited)},
                   {"entries_accepted", std::to_string(result.entriesAccepted)},
                   {"token_count", std::to_string(result.session.tokens.size())}});
  } else {
    m_hasPendingSession = false;
    m_pendingTokens.clear();
    m_pendingEntriesVisited = 0;
    m_pendingEntriesAccepted = 0;
    m_pendingMessage.clear();
    logging::error("app", "session_import_bridge", "analyze_failed",
                   "HAR analysis failed from QML", {{"reason", result.message}});
  }

  out.insert("ok", result.ok);
  out.insert("message", QString::fromStdString(result.message));
  out.insert("entriesVisited", static_cast<qulonglong>(result.entriesVisited));
  out.insert("entriesAccepted", static_cast<qulonglong>(result.entriesAccepted));
  out.insert("tokenKeys", tokenKeyList(result.session.tokens));
  out.insert("pendingValid", result.ok);
  out.insert("sessionPath", QString::fromStdString(
      cloud::resolveSessionPath(pathOverride).string()));
  finalizeUiMessage(out);
  return out;
}

QVariantMap SessionImportBridge::commitPendingSession(const QString& sessionPath) {
  QVariantMap out;
  if (!m_hasPendingSession || m_pendingTokens.empty()) {
    out.insert("ok", false);
    out.insert("message", "No valid HAR analysis pending.");
    out.insert("connectionOk", false);
    finalizeUiMessage(out);
    return out;
  }

  cloud::SessionData session;
  session.tokens = m_pendingTokens;
  const std::optional<std::filesystem::path> pathOverride = optionalFsPath(sessionPath);
  const cloud::SaveSessionResult saved = cloud::saveSessionFile(session, pathOverride);
  out.insert("ok", saved.ok);
  out.insert("message", QString::fromStdString(saved.message));
  out.insert("savedPath", QString::fromStdString(saved.path.string()));
  out.insert("entriesVisited", static_cast<qulonglong>(m_pendingEntriesVisited));
  out.insert("entriesAccepted", static_cast<qulonglong>(m_pendingEntriesAccepted));
  out.insert("connectionOk", false);

  if (!saved.ok) {
    logging::error("app", "session_import_bridge", "commit_failed",
                   "Failed to persist analyzed session", {{"path", saved.path.string()}});
    finalizeUiMessage(out);
    return out;
  }

  const auto accIt = m_pendingTokens.find("access_token");
  if (accIt == m_pendingTokens.end()) {
    out.insert("message", "Session saved, but access_token is missing.");
  } else {
    const cloud::CloudCheckResult check =
        cloud::checkCloudAuth(accIt->second, pickXxToken(m_pendingTokens));
    out.insert("connectionOk", check.ok);
    out.insert("connectionMessage", QString::fromStdString(check.message));
  }

  logging::info("app", "session_import_bridge", "commit_succeeded",
                "Analyzed session persisted from QML",
                {{"path", saved.path.string()},
                 {"entries_visited", std::to_string(m_pendingEntriesVisited)},
                 {"entries_accepted", std::to_string(m_pendingEntriesAccepted)},
                 {"token_count", std::to_string(m_pendingTokens.size())}});

  m_hasPendingSession = false;
  m_pendingTokens.clear();
  m_pendingEntriesVisited = 0;
  m_pendingEntriesAccepted = 0;
  m_pendingMessage.clear();
  finalizeUiMessage(out);
  return out;
}

void SessionImportBridge::discardPendingSession() {
  m_hasPendingSession = false;
  m_pendingTokens.clear();
  m_pendingEntriesVisited = 0;
  m_pendingEntriesAccepted = 0;
  m_pendingMessage.clear();
}

QVariantMap SessionImportBridge::sessionDetails(const QString& sessionPath) const {
  QVariantMap out;
  const std::optional<std::filesystem::path> pathOverride = optionalFsPath(sessionPath);
  const cloud::LoadSessionResult loaded = cloud::loadSessionFile(pathOverride);

  QStringList lines;
  lines << QStringLiteral("Path: %1").arg(QString::fromStdString(loaded.path.string()));
  lines << QStringLiteral("Exists: %1").arg(loaded.ok ? "true" : "false");
  if (!loaded.ok) {
    lines << QStringLiteral("Message: %1").arg(QString::fromStdString(loaded.message));
    out.insert("ok", false);
    out.insert("exists", false);
    out.insert("message", QString::fromStdString(loaded.message));
    out.insert("details", lines.join('\n'));
    finalizeUiMessage(out);
    return out;
  }

  lines << QStringLiteral("Token count: %1").arg(loaded.session.tokens.size());
  std::vector<std::string> keys;
  keys.reserve(loaded.session.tokens.size());
  for (const auto& [key, _] : loaded.session.tokens) {
    keys.push_back(key);
  }
  std::sort(keys.begin(), keys.end());
  for (const std::string& key : keys) {
    const auto it = loaded.session.tokens.find(key);
    if (it == loaded.session.tokens.end()) {
      continue;
    }
    lines << QStringLiteral("- %1: %2")
                 .arg(QString::fromStdString(key), truncateSecretValue(it->second));
  }

  out.insert("ok", true);
  out.insert("exists", true);
  out.insert("message", QStringLiteral("Session loaded"));
  out.insert("tokenKeys", tokenKeyList(loaded.session.tokens));
  out.insert("details", lines.join('\n'));
  finalizeUiMessage(out);
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
    finalizeUiMessage(out);
    return out;
  }
  out.insert("sessionExists", true);

  // 2. Récupérer le token d'accès
  const auto accIt = loaded.session.tokens.find("access_token");
  if (accIt == loaded.session.tokens.end()) {
    out.insert("connectionOk", false);
    out.insert("message", QString("session.json ne contient pas de access_token."));
    finalizeUiMessage(out);
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
  finalizeUiMessage(out);
  return out;
}

} // namespace accloud
