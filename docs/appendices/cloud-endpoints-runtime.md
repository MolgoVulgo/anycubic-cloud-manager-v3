# Cloud endpoint runtime matrix

> Status: ACTIVE. Source of truth: `src/accloud/infra/cloud/core/EndpointRegistry.cpp` plus the active API/use-case owners.

All registry entries currently require bearer authentication and Workbench signing. `POST` entries use the content type shown below; `GET` entries have no request content type.

| Endpoint ID | Method | Runtime path | Content type | Timeout | Active owner / role |
| --- | --- | --- | --- | ---: | --- |
| `AuthCheckSession` | POST | `/p/p/workbench/api/work/index/getUserStore` | JSON | 10 s | `SessionProvider`, legacy auth validation |
| `AuthLoginWithAccessToken` | POST | `/p/p/workbench/api/v3/public/loginWithAccessToken` | JSON | 10 s | legacy session bootstrap |
| `FilesList` | POST | `/p/p/workbench/api/work/index/files` | JSON | 10 s | `FilesApi` / cloud file listing |
| `FilesListFallback` | POST | `/p/p/workbench/api/work/index/userFiles` | JSON | 10 s | fallback file listing |
| `FilesDelete` | POST | `/p/p/workbench/api/work/index/delFiles` | JSON | 10 s | `FilesApi::remove` |
| `FilesDownloadUrl` | POST | `/p/p/workbench/api/work/index/getDowdLoadUrl` | JSON | 10 s | `DownloadsApi::getSignedUrl` |
| `UploadLockStorage` | POST | `/p/p/workbench/api/v2/cloud_storage/lockStorageSpace` | JSON | 15 s | `UploadsApi::lockStorageSpace` |
| `UploadRegisterFile` | POST | `/p/p/workbench/api/v2/profile/newUploadFile` | JSON | 10 s | `UploadsApi::registerUploadedFile` |
| `UploadStatus` | POST | `/p/p/workbench/api/work/index/getUploadStatus` | JSON | 10 s | `UploadsApi::getUploadStatus` |
| `UploadUnlockStorage` | POST | `/p/p/workbench/api/v2/cloud_storage/unlockStorageSpace` | JSON | 10 s | `UploadsApi::unlockStorageSpace` |
| `PrintersList` | GET | `/p/p/workbench/api/work/printer/getPrinters` | — | 10 s | `PrintersApi::list` |
| `PrintersStatus` | GET | `/p/p/workbench/api/v2/printer/printersStatus` | — | 10 s | compatibility/status lookups |
| `PrintersDetails` | GET | `/p/p/workbench/api/v2/printer/info` | — | 10 s | `PrintersApi::details` |
| `ProjectsListByPrinter` | GET | `/p/p/workbench/api/work/project/getProjects` | — | 10 s | `ProjectsApi::listByPrinter` |
| `ReasonCatalog` | GET | `/p/p/workbench/api/portal/index/reason` | — | 10 s | `ReasonCatalogApi::list` |
| `OrdersSend` | POST | `/p/p/workbench/api/work/operation/sendOrder` | form URL encoded | 10 s | `PrintOrderApi` print/printer commands |

## Requests outside the Workbench registry

Two transfers deliberately bypass Workbench signing after a signed URL is obtained:

```text
file download URL -> direct GET, no Workbench headers
preSignUrl         -> direct binary PUT, no Workbench envelope
```

The URLs are runtime secrets. They must not be persisted or logged in full.

## Maintenance rule

A path, method, signature, header, envelope or timeout must not be changed from generic REST assumptions. Update this matrix only when the active runtime changes, and validate the owning API/use case in the same patch.
