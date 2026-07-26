# Matrice runtime des endpoints cloud

> Statut : ACTIF. Source de vérité : `src/accloud/infra/cloud/core/EndpointRegistry.cpp` complété par les propriétaires API/use cases actifs.

Toutes les entrées du registre exigent actuellement bearer et signature Workbench. Les entrées `POST` utilisent le content type indiqué ; les entrées `GET` n'ont pas de content type de requête.

| Endpoint ID | Méthode | Path runtime | Content type | Timeout | Propriétaire actif / rôle |
| --- | --- | --- | --- | ---: | --- |
| `AuthCheckSession` | POST | `/p/p/workbench/api/work/index/getUserStore` | JSON | 10 s | `SessionProvider`, validation auth legacy |
| `AuthLoginWithAccessToken` | POST | `/p/p/workbench/api/v3/public/loginWithAccessToken` | JSON | 10 s | bootstrap session legacy |
| `FilesList` | POST | `/p/p/workbench/api/work/index/files` | JSON | 10 s | `FilesApi` / listing fichiers cloud |
| `FilesListFallback` | POST | `/p/p/workbench/api/work/index/userFiles` | JSON | 10 s | fallback du listing fichiers |
| `FilesDelete` | POST | `/p/p/workbench/api/work/index/delFiles` | JSON | 10 s | `FilesApi::remove` |
| `FilesDownloadUrl` | POST | `/p/p/workbench/api/work/index/getDowdLoadUrl` | JSON | 10 s | `DownloadsApi::getSignedUrl` |
| `UploadLockStorage` | POST | `/p/p/workbench/api/v2/cloud_storage/lockStorageSpace` | JSON | 15 s | `UploadsApi::lockStorageSpace` |
| `UploadRegisterFile` | POST | `/p/p/workbench/api/v2/profile/newUploadFile` | JSON | 10 s | `UploadsApi::registerUploadedFile` |
| `UploadStatus` | POST | `/p/p/workbench/api/work/index/getUploadStatus` | JSON | 10 s | `UploadsApi::getUploadStatus` |
| `UploadUnlockStorage` | POST | `/p/p/workbench/api/v2/cloud_storage/unlockStorageSpace` | JSON | 10 s | `UploadsApi::unlockStorageSpace` |
| `PrintersList` | GET | `/p/p/workbench/api/work/printer/getPrinters` | — | 10 s | `PrintersApi::list` |
| `PrintersStatus` | GET | `/p/p/workbench/api/v2/printer/printersStatus` | — | 10 s | lookups compatibilité/statut |
| `PrintersDetails` | GET | `/p/p/workbench/api/v2/printer/info` | — | 10 s | `PrintersApi::details` |
| `ProjectsListByPrinter` | GET | `/p/p/workbench/api/work/project/getProjects` | — | 10 s | `ProjectsApi::listByPrinter` |
| `ReasonCatalog` | GET | `/p/p/workbench/api/portal/index/reason` | — | 10 s | `ReasonCatalogApi::list` |
| `OrdersSend` | POST | `/p/p/workbench/api/work/operation/sendOrder` | form URL encoded | 10 s | `PrintOrderApi`, commandes print/imprimante |

## Requêtes hors registre Workbench

Deux transferts contournent volontairement la signature Workbench après obtention d'une URL signée :

```text
URL téléchargement fichier -> GET direct, sans headers Workbench
preSignUrl                   -> PUT binaire direct, sans enveloppe Workbench
```

Ces URLs sont des secrets runtime. Elles ne doivent être ni persistées ni loggées intégralement.

## Règle de maintenance

Ne pas modifier path, méthode, signature, headers, enveloppe ou timeout au nom de conventions REST génériques. Mettre à jour cette matrice uniquement avec le runtime actif et valider le propriétaire API/use case dans le même correctif.
