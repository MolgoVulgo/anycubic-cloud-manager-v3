#include "CloudBridge.h"

#include "infra/cloud/CloudClient.h"
#include "infra/cloud/HarImporter.h"
#include "infra/logging/JsonlLogger.h"

#include <QNetworkRequest>
#include <QUrl>
#include <QVariantList>

#include <cmath>
#include <string>

namespace accloud {
namespace {

// ── Formatage taille ──────────────────────────────────────────────────────

QString formatBytes(uint64_t bytes) {
    if (bytes >= uint64_t{1} << 30)
        return QString::number(bytes / double(uint64_t{1} << 30), 'f', 1) + " GB";
    if (bytes >= uint64_t{1} << 20)
        return QString::number(bytes / double(uint64_t{1} << 20), 'f', 1) + " MB";
    if (bytes >= uint64_t{1} << 10)
        return QString::number(bytes / double(uint64_t{1} << 10), 'f', 1) + " KB";
    return QString::number(bytes) + " B";
}

// ── Formatage statut ──────────────────────────────────────────────────────

QString formatStatus(int status) {
    switch (status) {
        case 1:  return QStringLiteral("READY");
        case 2:  return QStringLiteral("PROCESSING");
        default: return QStringLiteral("UNKNOWN");
    }
}

// ── Conversion CloudFileInfo → QVariantMap ────────────────────────────────

QVariantMap fileInfoToMap(const cloud::CloudFileInfo& f) {
    const QString name = QString::fromStdString(f.name);
    const bool isPwmb  = name.endsWith(".pwmb", Qt::CaseInsensitive)
                      || name.endsWith(".pwmb", Qt::CaseInsensitive);

    QVariantMap m;
    m.insert("fileId",        QString::fromStdString(f.id));
    m.insert("fileName",      name);
    m.insert("status",        formatStatus(f.status));
    m.insert("sizeText",      formatBytes(f.sizeBytes));
    m.insert("machine",       QString::fromStdString(f.machine));
    m.insert("material",      QString::fromStdString(f.material));
    m.insert("uploadTime",    QString{});  // pas disponible dans le listing
    m.insert("printTime",     QString::fromStdString(f.printTime));
    m.insert("layerThickness",QString::fromStdString(f.layerHeight));
    m.insert("layers",        f.layers.empty() ? 0 : std::stoi(f.layers));
    m.insert("isPwmb",        isPwmb);
    m.insert("resinUsage",    QString::fromStdString(f.resinUsage));
    m.insert("dimensions",    QString::fromStdString(f.dimensions));
    m.insert("thumbnailUrl",  QString::fromStdString(f.thumbnailUrl));
    m.insert("gcodeId",       QString::fromStdString(f.gcodeId));
    return m;
}

QVariantMap printerInfoToMap(const cloud::CloudPrinterInfo& p) {
    QVariantMap m;
    m.insert("id",          QString::fromStdString(p.id));
    m.insert("name",        QString::fromStdString(p.name));
    m.insert("model",       QString::fromStdString(p.model));
    m.insert("state",       QString::fromStdString(p.state));
    m.insert("reason",      QString::fromStdString(p.reason));
    m.insert("available",   p.available);
    m.insert("progress",    p.progress);
    m.insert("elapsedSec",  p.elapsedSec);
    m.insert("remainingSec",p.remainingSec);
    m.insert("currentFile", QString::fromStdString(p.currentFile));
    return m;
}

QVariantMap printerCompatToMap(const cloud::CloudPrinterCompatItem& p) {
    QVariantMap m;
    m.insert("id",        QString::fromStdString(p.id));
    m.insert("available", p.available);
    m.insert("reason",    QString::fromStdString(p.reason));
    return m;
}

} // namespace

// ── Constructeur / destructeur ────────────────────────────────────────────

CloudBridge::CloudBridge(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this)) {}

CloudBridge::~CloudBridge() {
    cleanupDownload();
}

// ── loadTokens ────────────────────────────────────────────────────────────

bool CloudBridge::loadTokens(std::string& accessToken, std::string& xxToken) const {
    const auto loaded = cloud::loadSessionFile();
    if (!loaded.ok) {
        logging::warn("app", "cloud_bridge", "load_tokens_failed",
                      "Session introuvable", {{"path", loaded.path.string()}});
        return false;
    }
    const auto accIt = loaded.session.tokens.find("access_token");
    if (accIt == loaded.session.tokens.end()) {
        logging::warn("app", "cloud_bridge", "load_tokens_no_access",
                      "access_token absent de la session");
        return false;
    }
    accessToken = accIt->second;
    const auto tokIt = loaded.session.tokens.find("token");
    xxToken = (tokIt != loaded.session.tokens.end()) ? tokIt->second : std::string{};
    return true;
}

// ── fetchFiles ────────────────────────────────────────────────────────────

QVariantMap CloudBridge::fetchFiles(int page, int limit) const {
    QVariantMap out;
    std::string at, tok;
    if (!loadTokens(at, tok)) {
        out.insert("ok", false);
        out.insert("message", QString("Session invalide ou introuvable."));
        return out;
    }

    logging::info("app", "cloud_bridge", "fetch_files_start", "Chargement des fichiers cloud",
                  {{"page", std::to_string(page)}, {"limit", std::to_string(limit)}});

    const auto res = cloud::fetchCloudFiles(at, tok, page, limit);
    out.insert("ok",      res.ok);
    out.insert("message", QString::fromStdString(res.message));
    out.insert("total",   res.total);

    if (res.ok) {
        QVariantList files;
        files.reserve(static_cast<qsizetype>(res.files.size()));
        for (const auto& f : res.files)
            files.append(fileInfoToMap(f));
        out.insert("files", files);
    }
    return out;
}

// ── fetchQuota ────────────────────────────────────────────────────────────

QVariantMap CloudBridge::fetchQuota() const {
    QVariantMap out;
    std::string at, tok;
    if (!loadTokens(at, tok)) {
        out.insert("ok", false);
        out.insert("message", QString("Session invalide."));
        return out;
    }

    const auto q = cloud::fetchCloudQuota(at, tok);
    out.insert("ok",           q.ok);
    out.insert("message",      QString::fromStdString(q.message));
    out.insert("totalDisplay", QString::fromStdString(q.totalDisplay));
    out.insert("usedDisplay",  QString::fromStdString(q.usedDisplay));
    out.insert("totalBytes",   static_cast<qulonglong>(q.totalBytes));
    out.insert("usedBytes",    static_cast<qulonglong>(q.usedBytes));
    return out;
}

// ── deleteFile ────────────────────────────────────────────────────────────

QVariantMap CloudBridge::deleteFile(const QString& fileId) const {
    QVariantMap out;
    std::string at, tok;
    if (!loadTokens(at, tok)) {
        out.insert("ok", false);
        out.insert("message", QString("Session invalide."));
        return out;
    }

    logging::info("app", "cloud_bridge", "delete_file_start", "Suppression fichier",
                  {{"file_id", fileId.toStdString()}});
    const auto r = cloud::deleteCloudFile(at, tok, fileId.toStdString());
    out.insert("ok",      r.ok);
    out.insert("message", QString::fromStdString(r.message));
    return out;
}

// ── getDownloadUrl ────────────────────────────────────────────────────────

QVariantMap CloudBridge::getDownloadUrl(const QString& fileId) const {
    QVariantMap out;
    std::string at, tok;
    if (!loadTokens(at, tok)) {
        out.insert("ok", false);
        out.insert("message", QString("Session invalide."));
        return out;
    }

    const auto r = cloud::getCloudDownloadUrl(at, tok, fileId.toStdString());
    out.insert("ok",      r.ok);
    out.insert("message", QString::fromStdString(r.message));
    if (r.ok)
        out.insert("url", QString::fromStdString(r.url));
    return out;
}

// ── fetchPrinters ─────────────────────────────────────────────────────────

QVariantMap CloudBridge::fetchPrinters() const {
    QVariantMap out;
    std::string at, tok;
    if (!loadTokens(at, tok)) {
        out.insert("ok", false);
        out.insert("message", QString("Session invalide."));
        return out;
    }

    const auto r = cloud::fetchCloudPrinters(at, tok);
    out.insert("ok",      r.ok);
    out.insert("message", QString::fromStdString(r.message));
    if (r.ok) {
        QVariantList printers;
        printers.reserve(static_cast<qsizetype>(r.printers.size()));
        for (const auto& p : r.printers)
            printers.append(printerInfoToMap(p));
        out.insert("printers", printers);
    }
    return out;
}

// ── fetchCompatiblePrintersByExt ─────────────────────────────────────────

QVariantMap CloudBridge::fetchCompatiblePrintersByExt(const QString& fileExt) const {
    QVariantMap out;
    std::string at, tok;
    if (!loadTokens(at, tok)) {
        out.insert("ok", false);
        out.insert("message", QString("Session invalide."));
        return out;
    }

    const auto r = cloud::fetchPrinterCompatibilityByExt(
        at, tok, fileExt.trimmed().toLower().toStdString());
    out.insert("ok",      r.ok);
    out.insert("message", QString::fromStdString(r.message));
    if (r.ok) {
        QVariantList printers;
        printers.reserve(static_cast<qsizetype>(r.printers.size()));
        for (const auto& p : r.printers)
            printers.append(printerCompatToMap(p));
        out.insert("printers", printers);
    }
    return out;
}

// ── sendPrintOrder ────────────────────────────────────────────────────────

QVariantMap CloudBridge::sendPrintOrder(const QString& printerId,
                                        const QString& fileId,
                                        bool deleteAfterPrint,
                                        bool dryRun) const {
    QVariantMap out;
    if (dryRun) {
        out.insert("ok", true);
        out.insert("message", QString("Dry-run: print order payload generated."));
        out.insert("taskId", QString());
        return out;
    }

    std::string at, tok;
    if (!loadTokens(at, tok)) {
        out.insert("ok", false);
        out.insert("message", QString("Session invalide."));
        return out;
    }

    const auto r = cloud::sendCloudPrintOrder(
        at, tok, printerId.toStdString(), fileId.toStdString(), deleteAfterPrint);
    out.insert("ok",      r.ok);
    out.insert("message", QString::fromStdString(r.message));
    out.insert("taskId",  QString::fromStdString(r.taskId));
    return out;
}

// ── startDownload (async) ─────────────────────────────────────────────────

void CloudBridge::startDownload(const QString& signedUrl, const QString& savePath) {
    if (m_dlReply) {
        logging::warn("app", "cloud_bridge", "download_already_running",
                      "Un téléchargement est déjà en cours");
        return;
    }

    m_dlPath = savePath;
    m_dlFile = new QFile(savePath, this);
    if (!m_dlFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        logging::error("app", "cloud_bridge", "download_open_failed",
                       "Impossible d'ouvrir le fichier de destination",
                       {{"path", savePath.toStdString()}});
        emit downloadFinished(false, "Impossible d'ouvrir : " + savePath, {});
        delete m_dlFile;
        m_dlFile = nullptr;
        return;
    }

    logging::info("app", "cloud_bridge", "download_start",
                  "Démarrage téléchargement",
                  {{"dest", savePath.toStdString()}});

    // GET direct sur l'URL signée — SANS Authorization ni XX-* (section 4.3 / 6.2)
    const QUrl dlUrl(signedUrl);
    QNetworkRequest req(dlUrl);
    req.setTransferTimeout(0);  // pas de timeout pour les gros fichiers

    m_dlReply = m_nam->get(req);

    connect(m_dlReply, &QNetworkReply::readyRead, this, [this]() {
        if (m_dlFile) m_dlFile->write(m_dlReply->readAll());
    });

    connect(m_dlReply, &QNetworkReply::downloadProgress,
            this, [this](qint64 recv, qint64 total) {
                emit downloadProgress(recv, total);
            });

    connect(m_dlReply, &QNetworkReply::finished, this, [this]() {
        const bool netOk = (m_dlReply->error() == QNetworkReply::NoError);
        const QString errStr = m_dlReply->errorString();
        const QString path   = m_dlPath;

        if (m_dlFile) {
            m_dlFile->flush();
            m_dlFile->close();
        }

        if (netOk) {
            logging::info("app", "cloud_bridge", "download_finished_ok",
                          "Téléchargement terminé", {{"dest", path.toStdString()}});
            emit downloadFinished(true, "Téléchargement terminé", path);
        } else {
            logging::warn("app", "cloud_bridge", "download_failed",
                          "Échec téléchargement", {{"error", errStr.toStdString()}});
            if (m_dlFile) m_dlFile->remove();
            emit downloadFinished(false, "Erreur: " + errStr, {});
        }
        cleanupDownload();
    });
}

// ── cancelDownload ────────────────────────────────────────────────────────

void CloudBridge::cancelDownload() {
    if (m_dlReply) {
        logging::info("app", "cloud_bridge", "download_cancelled",
                      "Annulation téléchargement");
        m_dlReply->abort();
        // cleanupDownload() sera appelé via le signal finished
    }
}

// ── cleanupDownload ───────────────────────────────────────────────────────

void CloudBridge::cleanupDownload() {
    if (m_dlReply) {
        m_dlReply->deleteLater();
        m_dlReply = nullptr;
    }
    if (m_dlFile) {
        delete m_dlFile;
        m_dlFile = nullptr;
    }
    m_dlPath.clear();
}

} // namespace accloud
