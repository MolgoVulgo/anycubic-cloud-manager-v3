# core-web implementation lots

## Objectif

Découper l’implémentation core-web en lots livrables, testables et indépendants, avec périmètre clair.

## Hypothèses

- Le comportement fonctionnel UI actuel doit rester stable pendant la migration.
- Les changements sont réalisés sur la branche `core-web`.
- Les lots visent des PRs courtes et auditables.

## Lot 1 - Noyau web minimal (pipeline unique)

### But

Créer la colonne vertébrale technique commune sans modifier la logique métier.

### Tickets

1. Créer `infra/cloud/core/HttpClient` (GET/POST/POST form, timeout, erreurs transport).
2. Créer `infra/cloud/core/EndpointRegistry` (id logique, méthode, path, auth, signature, content type).
3. Créer `infra/cloud/core/WorkbenchRequestBuilder` (construction URL/headers/body).
4. Introduire `infra/cloud/core/WorkbenchSigner` (migration de `SignHeaders`).
5. Créer `infra/cloud/core/ResponseEnvelopeParser` (`HTTP + code/msg/data`).

### Fichiers impactés (cible)

- `accloud/infra/cloud/core/HttpClient.{h,cpp}`
- `accloud/infra/cloud/core/EndpointRegistry.{h,cpp}`
- `accloud/infra/cloud/core/WorkbenchRequestBuilder.{h,cpp}`
- `accloud/infra/cloud/core/WorkbenchSigner.{h,cpp}`
- `accloud/infra/cloud/core/ResponseEnvelopeParser.{h,cpp}`
- `accloud/CMakeLists.txt`

### Critères d’acceptation

- Tous les appels cloud passent par le pipeline core-web.
- Aucune régression visible sur login/listing cloud.
- Tests unitaires noyau verts.

### Estimation

- 2 à 3 jours.

---

## Lot 2 - Session/auth centralisées

### But

Supprimer la lecture de session dispersée et centraliser l’auth.

### Tickets

1. Créer `SessionProvider` (`loadCurrentSession`, `requireAccessToken`, `optionalXxToken`).
2. Remplacer les accès directs `loadSessionFile()` dans bridges/appels infra.
3. Uniformiser les erreurs auth (`missing token`, `expired`, `invalid`).

### Fichiers impactés (cible)

- `accloud/infra/cloud/core/SessionProvider.{h,cpp}`
- `accloud/infra/cloud/*` (appelants existants)
- `accloud/app/CloudBridge.cpp`

### Critères d’acceptation

- Plus aucun accès direct à `session.json` hors provider.
- Les endpoints reçoivent un `CloudRequestContext` unique.
- Les erreurs auth sont typées et cohérentes.

### Estimation

- 1 à 2 jours.

---

## Lot 3 - Extraction des APIs spécialisées

### But

Découper `CloudClient` en services endpoint-oriented.

### Tickets

1. Créer `AuthApi`, `FilesApi`, `QuotaApi`.
2. Créer `PrintersApi`, `ProjectsApi`, `ReasonCatalogApi`.
3. Créer `DownloadsApi`, `PrintOrderApi`.
4. Réduire `CloudClient` à une façade mince (ou suppression contrôlée).

### Fichiers impactés (cible)

- `accloud/infra/cloud/api/*.h|*.cpp` (nouveaux)
- `accloud/infra/cloud/CloudClient.{h,cpp}` (réduction/dissolution)
- `accloud/CMakeLists.txt`

### Critères d’acceptation

- Chaque API a un périmètre endpoint clair.
- Les parsers JSON sont localisés dans les APIs.
- `CloudClient.cpp` n’est plus un point de concentration métier.

### Estimation

- 3 à 5 jours.

---

## Lot 4 - Use cases cloud

### But

Déplacer l’orchestration métier hors UI.

### Tickets

1. Créer `LoadCloudFilesUseCase`.
2. Créer `LoadPrintersDashboardUseCase`.
3. Créer `CheckCloudSessionUseCase`.
4. Créer `SendPrintOrderUseCase`.
5. Créer `GetDownloadUrlUseCase` et `DownloadCloudFileUseCase`.

### Fichiers impactés (cible)

- `accloud/app/usecases/cloud/*.h|*.cpp` (nouveaux)
- `accloud/app/CloudBridge.{h,cpp}` (délégation)
- `accloud/CMakeLists.txt`

### Critères d’acceptation

- Les règles de retry/fallback sont dans usecases, pas dans bridge.
- Les usecases retournent des résultats typés exploitables.
- Flux fonctionnels cloud inchangés côté utilisateur.

### Estimation

- 3 à 4 jours.

---

## Lot 5 - Amincissement de CloudBridge

### But

Transformer `CloudBridge` en simple adaptateur QML.

### Tickets

1. Retirer session/retry/orchestration réseau du bridge.
2. Conserver uniquement validation paramètres, appel usecase, mapping `QVariant`.
3. Stabiliser signaux UI (`busy`, `error`, `data ready`).

### Fichiers impactés (cible)

- `accloud/app/CloudBridge.{h,cpp}`
- `accloud/tests/ui/*` (tests bridge)

### Critères d’acceptation

- `CloudBridge` ne contient plus de logique d’agrégation multi-endpoints.
- Les tests UI bridge valident uniquement adaptation et signaux.

### Estimation

- 2 jours.

---

## Lot 6 - Orchestrateur imprimantes unique

### But

Supprimer la duplication d’agrégation imprimantes.

### Tickets

1. Consolider la vue imprimantes dans `LoadPrintersDashboardUseCase`.
2. Éliminer enrichissements redondants (`fetchCloudPrinters` vs `fetchPrintersWithRetry`).
3. Définir une règle unique de fusion (`progress`, `currentFile`, `layers`, statut).

### Fichiers impactés (cible)

- `accloud/app/usecases/cloud/LoadPrintersDashboardUseCase.{h,cpp}`
- `accloud/infra/cloud/api/PrintersApi.*`
- `accloud/infra/cloud/api/ProjectsApi.*`
- `accloud/app/CloudBridge.cpp`

### Critères d’acceptation

- Une seule couche compose le dashboard imprimantes.
- Plus de double appel réseau non justifié.
- Résultats stables sur cas partiellement dégradés (un sous-endpoint KO).

### Estimation

- 2 à 3 jours.

---

## Lot 7 - Cache et téléchargement réalignés infra

### But

Sortir les détails infra hors couche app/bridge.

### Tickets

1. Déplacer `LocalCacheStore` vers `infra/cache/sqlite`.
2. Introduire `CloudCacheRepository` (port applicatif).
3. Séparer `DownloadsApi::getSignedUrl` de `BinaryDownloader::download`.

### Fichiers impactés (cible)

- `accloud/infra/cache/sqlite/LocalCacheStore.{h,cpp}` (move)
- `accloud/app/LocalCacheStore.*` (suppression/migration)
- `accloud/infra/cloud/api/DownloadsApi.*`
- `accloud/app/usecases/cloud/*Download*`

### Critères d’acceptation

- Le bridge UI n’accède plus directement au cache.
- Les téléchargements ont un flux clair en 2 étapes.
- Pas de fuite d’URL signée dans les logs.

### Estimation

- 2 jours.

---

## Lot 8 - Jobs cloud et durcissement runtime

### But

Remplacer les threads détachés et finaliser le cycle de vie des opérations cloud.

### Tickets

1. Brancher `infra/jobs/Jobs_Cloud.*` sur les usecases cloud.
2. Ajouter annulation/priorité/sérialisation des jobs.
3. Supprimer `std::thread(...).detach()` dans le flux cloud.

### Fichiers impactés (cible)

- `accloud/infra/jobs/Jobs_Cloud.*`
- `accloud/infra/jobs/JobManager.*`
- `accloud/app/usecases/cloud/*`
- `accloud/app/CloudBridge.cpp`

### Critères d’acceptation

- Plus aucun thread détaché piloté par bridge.
- Progress/cancel/retry suivent un orchestrateur unique.
- Shutdown application propre sans tâches orphelines.

### Estimation

- 2 à 3 jours.

---

## Plan de tests par lot

1. Unit core-web: `WorkbenchSigner`, `EndpointRegistry`, `ResponseEnvelopeParser`, `SessionProvider`.
2. Unit API: `FilesApi`, `PrintersApi`, `ProjectsApi`, `PrintOrderApi`.
3. Unit usecases: files/printers/session/order.
4. Intégration sans QML: flux end-to-end mocké.
5. UI bridge non-régression: mapping/signal uniquement.

## Stratégie de livraison

1. Un lot = une PR.
2. Chaque PR inclut docs de périmètre + tests.
3. Merge séquentiel dans l’ordre des lots.
4. Interdiction d’ouvrir un lot N+1 tant que les critères du lot N ne sont pas validés.

## Ordre recommandé (strict)

1. Lot 1
2. Lot 2
3. Lot 3
4. Lot 4
5. Lot 5
6. Lot 6
7. Lot 7
8. Lot 8
