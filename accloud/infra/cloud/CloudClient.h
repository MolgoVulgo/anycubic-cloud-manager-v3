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
    std::string gcodeId;
    std::string thumbnailUrl;
    int         status       = 0;  // 0=inconnu, 1=prêt, 2=traitement...
    // Champs slice_param (best-effort, peuvent être vides) :
    std::string machine;
    std::string material;
    std::string printTime;    // ex "06h 32m"
    std::string layerHeight;  // ex "0.05 mm"
    std::string layers;       // ex "2586"
    std::string resinUsage;   // ex "118 ml"
    std::string dimensions;   // ex "120x74x148"
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
    std::string name;
    std::string model;
    std::string state;       // OFFLINE | READY | PRINTING | ERROR
    std::string reason;
    int available = 0;
    int progress = -1;       // 0..100, -1 si indisponible
    int elapsedSec = -1;
    int remainingSec = -1;
    std::string currentFile;
};

struct CloudPrintersResult {
    bool ok = false;
    std::string message;
    std::vector<CloudPrinterInfo> printers;
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

// Démarrage d'impression distante depuis un fichier cloud
CloudPrintOrderResult sendCloudPrintOrder(const std::string& accessToken,
                                          const std::string& xxToken,
                                          const std::string& printerId,
                                          const std::string& fileId,
                                          bool deleteAfterPrint);

} // namespace accloud::cloud
