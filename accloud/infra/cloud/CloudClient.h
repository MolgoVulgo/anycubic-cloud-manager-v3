#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace accloud::cloud {

// --- Auth check -------------------------------------------------------

struct CloudCheckResult {
    bool ok{false};
    std::string message;
};

// Vérifie d'abord la session via POST getUserStore, puis fallback loginWithAccessToken.
// accessToken : session.tokens["access_token"]
// xxToken     : session.tokens["token"] (optionnel, pour XX-Token)
// Retourne ok=true si le serveur répond avec code:1.
CloudCheckResult checkCloudAuth(const std::string& accessToken,
                                const std::string& xxToken = {});

// --- Fichiers cloud ---------------------------------------------------

// Métadonnées d'un fichier (listing + slice_param best-effort)
struct CloudFileInfo {
    std::string id;
    std::string name;
    uint64_t    sizeBytes    = 0;
    long long   createTime   = 0;  // epoch sec (best-effort)
    long long   updateTime   = 0;  // epoch sec (best-effort)
    std::string gcodeId;
    std::string thumbnailUrl;
    std::string downloadUrl;
    std::string region;
    std::string bucket;
    std::string path;
    std::string md5;
    int         status       = 0;  // 0=inconnu, 1=prêt, 2=traitement...
    // Champs slice_param (best-effort, peuvent être vides) :
    std::string machine;
    std::string printers;
    std::string material;
    std::string printTime;    // ex "06h 32m"
    std::string layerHeight;  // ex "0.05 mm"
    std::string layers;       // ex "2586"
    std::string resinUsage;   // ex "118 ml"
    std::string dimensions;   // ex "120x74x148"
    std::string bottomLayers; // ex "4"
    std::string exposureTime; // ex "1.5 s"
    std::string offTime;      // ex "0.5 s"
};

struct CloudFilesResult {
    bool ok = false;
    std::string message;
    std::vector<CloudFileInfo> files;
    int total = 0;
};

struct CloudQuotaResult {
    bool ok = false;
    std::string message;
    std::string totalDisplay;  // "2.00GB"
    uint64_t    totalBytes  = 0;
    std::string usedDisplay;   // "1.13GB"
    uint64_t    usedBytes   = 0;
};

struct CloudOpResult {
    bool ok = false;
    std::string message;
};

struct CloudDownloadResult {
    bool ok = false;
    std::string message;
    std::string url;  // URL signée — ne pas logger !
};

// --- Imprimantes / remote print --------------------------------------

struct CloudPrinterInfo {
    std::string id;
    std::string printerKey;
    std::string machineType;
    std::string name;
    std::string model;
    std::string type;
    std::string lastSeen;
    std::string state;       // OFFLINE | READY | PRINTING | ERROR
    std::string reason;
    int available = 0;
    int progress = -1;       // 0..100, -1 si indisponible
    int elapsedSec = -1;
    int remainingSec = -1;
    int currentLayer = -1;
    int totalLayers = -1;
    std::string currentFile;
};

struct CloudPrintersResult {
    bool ok = false;
    std::string message;
    std::vector<CloudPrinterInfo> printers;
    std::string rawJson; // Debug only (ACCLOUD_DEBUG=ON)
};

struct CloudPrinterCompatItem {
    std::string id;
    int available = 0;
    std::string reason;
};

struct CloudPrinterCompatResult {
    bool ok = false;
    std::string message;
    std::vector<CloudPrinterCompatItem> printers;
};

struct CloudPrintOrderResult {
    bool ok = false;
    std::string message;
    std::string taskId;
    std::string msgId;
    std::string correlationTicket;
    std::string correlationStatus;
};

struct CloudPrinterDetailsResult {
    bool ok = false;
    std::string message;
    std::string rawJson; // Debug only (ACCLOUD_DEBUG=ON)
    int progress = -1;
    int elapsedSec = -1;
    int remainingSec = -1;
    int currentLayer = -1;
    int totalLayers = -1;
    std::string currentFile;
    std::string firmwareVersion;
    std::string printCount;
    std::string printTotalTime;
    std::string materialType;
    std::string materialUsed;
    std::string machineMac;
    std::string helpUrl;
    std::string quickStartUrl;
    std::string releaseFilmLayers;
    std::vector<std::string> tools;
    std::vector<std::string> advances;
};

struct CloudReasonCatalogItem {
    int reason = 0;
    std::string desc;
    std::string helpUrl;
    std::string type;
    int push = 0;
    int popup = 0;
};

struct CloudReasonCatalogResult {
    bool ok = false;
    std::string message;
    std::vector<CloudReasonCatalogItem> reasons;
};

struct CloudPrinterProjectItem {
    std::string taskId;
    std::string gcodeName;
    std::string printerId;
    std::string printerName;
    int printStatus = 0;
    int progress = -1;
    int elapsedSec = -1;
    int remainingSec = -1;
    int currentLayer = -1;
    int totalLayers = -1;
    std::string reason;
    std::string currentFile;
    long long createTime = 0;
    long long endTime = 0;
    std::string img;
};

struct CloudPrinterProjectsResult {
    bool ok = false;
    std::string message;
    std::string rawJson; // Debug only (ACCLOUD_DEBUG=ON)
    std::vector<CloudPrinterProjectItem> items;
};

// Listing fichiers (essaie /files, fallback /userFiles)
CloudFilesResult fetchCloudFiles(const std::string& accessToken,
                                 const std::string& xxToken,
                                 int page  = 1,
                                 int limit = 20);

// Quota de stockage utilisateur
CloudQuotaResult fetchCloudQuota(const std::string& accessToken,
                                 const std::string& xxToken);

// Suppression d'un fichier
CloudOpResult deleteCloudFile(const std::string& accessToken,
                              const std::string& xxToken,
                              const std::string& fileId);

// Obtenir l'URL signée de téléchargement (ne pas logger)
CloudDownloadResult getCloudDownloadUrl(const std::string& accessToken,
                                        const std::string& xxToken,
                                        const std::string& fileId);

// Liste des imprimantes associées au compte
CloudPrintersResult fetchCloudPrinters(const std::string& accessToken,
                                       const std::string& xxToken);

// Compatibilité des imprimantes par extension de fichier (pwmb/pws/...)
CloudPrinterCompatResult fetchPrinterCompatibilityByExt(const std::string& accessToken,
                                                        const std::string& xxToken,
                                                        const std::string& fileExt);

// Compatibilité des imprimantes par identifiant de fichier cloud
CloudPrinterCompatResult fetchPrinterCompatibilityByFileId(const std::string& accessToken,
                                                           const std::string& xxToken,
                                                           const std::string& fileId);

// Détails enrichis d'une imprimante
CloudPrinterDetailsResult fetchPrinterDetails(const std::string& accessToken,
                                              const std::string& xxToken,
                                              const std::string& printerId);

// Catalogue des raisons Anycubic (reason -> desc/help_url)
CloudReasonCatalogResult fetchReasonCatalog(const std::string& accessToken,
                                            const std::string& xxToken);

// Historique de projets par imprimante
CloudPrinterProjectsResult fetchPrinterProjects(const std::string& accessToken,
                                                const std::string& xxToken,
                                                const std::string& printerId,
                                                int page = 1,
                                                int limit = 10);

// Démarrage d'impression distante depuis un fichier cloud
CloudPrintOrderResult sendCloudPrintOrder(const std::string& accessToken,
                                          const std::string& xxToken,
                                          const std::string& printerId,
                                          const std::string& fileId,
                                          bool deleteAfterPrint);

// Envoi generique d'une commande sendOrder (printer local files, usb files, etc.)
CloudPrintOrderResult sendCloudPrinterOrder(const std::string& accessToken,
                                            const std::string& xxToken,
                                            const std::string& printerId,
                                            int orderId,
                                            const std::string& projectId,
                                            const std::string& dataJson = {});

} // namespace accloud::cloud
