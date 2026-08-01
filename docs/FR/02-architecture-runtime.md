# Architecture et runtime actif

## En bref

L'application suit une architecture C++ en couches. QML affiche l'état et transmet l'intention utilisateur ; le C++ conserve les use cases, protocoles, stockages et règles de sécurité.

```text
pages et dialogues QML
        ↓
bridges Qt et modèles UI
        ↓
use cases et store temps réel
        ↓
cloud / MQTT / cache / logs / formats
```

## Frontières des modules

| Chemin | Responsabilité |
| --- | --- |
| `src/accloud/app/` | bootstrap, bridges Qt, modèles UI et coordination des use cases |
| `src/accloud/domain/` | vocabulaire métier et contrats stables |
| `src/accloud/infra/` | cloud HTTP, MQTT, stockage, cache, logs et formats |
| `src/accloud/render3d/` | scaffold OpenGL/Qt Quick expérimental, exclu du runtime de production |
| `src/accloud/ui/qml/` | shell visuel, pages, dialogues et contrôles |
| `tests/` | tests de régression C++ et QML |

Une correction reste dans le module déjà propriétaire. Un problème cloud ne migre pas vers QML et un correctif cloud-only ne modifie ni MQTT ni render3d.

L’infrastructure cloud est découpée par propriétaire API. `CloudClient` reste le point d’entrée de compatibilité utilisé par les use cases applicatifs, tandis que `AuthApi`, `FilesApi`, `QuotaApi`, `DownloadsApi`, `PrintersApi`, `ProjectsApi`, `ReasonCatalogApi` et `PrintOrderApi` possèdent la construction des payloads et le parsing de leurs endpoints. `ApiSupport` est volontairement limité au transport Workbench et aux conversions JSON génériques. Il n’existe plus de backend partagé `CloudLegacyImpl`.

## Exécutable et points d'entrée

CMake construit l'exécutable partagé `accloud_cli`.

`src/accloud/app/main.cpp` sélectionne :

```text
--smoke ou --import-har
-> exécution CLI via App

aucun flag CLI et Qt disponible
-> QGuiApplication
-> création des bridges
-> qrc:/qml/MainWindow.qml
```

Le bootstrap desktop expose à QML `SessionImportBridge`, `CloudBridge`, `CloudFilesWorkflowBridge`, `PrintWorkflowBridge`, `MqttBridge`, `UiSettingsBridge`, `AppI18nBridge` et les modèles UI enregistrés. Il raccorde directement `MqttBridge::printerFileActionReceived` à `PrintWorkflowBridge::handlePrinterFileAction` afin que la confirmation du nettoyage d'une impression directe soit traitée en C++ plutôt qu'en QML. Il route également vers `PrintWorkflowBridge` les fins d'ordres imprimante, de suppression cloud et de contrôles de compatibilité d'impression distante provenant de `CloudBridge` ; les intentions de transport émises par le workflow d'impression sont ensuite retransmises à `CloudBridge`. La suppression des fichiers cloud passe séparément par `CloudFilesWorkflowBridge` : ce bridge porte la corrélation des suppressions unitaires et le cycle séquentiel des suppressions groupées adossé à `DeleteCloudFilesUseCase`, tandis que `CloudBridge` reste l'exécuteur transport/cache. QML reçoit donc uniquement une progression et une fin sémantiques, sans corréler les callbacks de suppression ni maintenir de file d'attente. Les objets debug n'existent que lorsque `ACCLOUD_DEBUG` est activé.

`CloudBridge` est désormais une façade de compatibilité fine plutôt que le propriétaire de tous les mécanismes cloud. Le mapping modèles-vers-`QVariant` et la normalisation des messages résident dans `CloudBridgeSupport` ; la résolution/cache des miniatures et l'exception TLS limitée aux miniatures résident dans `ThumbnailService` ; les transferts utilisateur via URL signée résident dans `CloudDownloadController` ; les cycles upload et mise à jour de preview PWSZ résident dans `CloudUploadController`. `CloudBridge` conserve le contrat QML public, la synchronisation du cache, la coordination des refresh cloud asynchrones et la délégation des commandes fichiers/imprimantes. Ce découpage maintient les téléchargements signés sans headers Workbench et empêche les détails image/TLS/PWSZ de remonter dans la façade exposée à l'UI.

`LocalCacheStore` reste la façade de compatibilité utilisée par l'application et les workflows, mais son implémentation SQLite est séparée par responsabilité. `LocalCacheSql` porte les helpers de connexion, la création du schéma et les migrations ; `LocalCacheFiles`, `LocalCachePrinters`, `LocalCacheJobs` et `LocalCacheState` portent leurs requêtes et transactions respectives. Le runtime et les tests de régression SQL compilent le même ensemble `ACCLOUD_LOCAL_CACHE_SOURCES`, afin que l'implémentation testée ne puisse pas diverger du build desktop.


`MqttBridge` reste la façade de compatibilité exposée à QML, mais son implémentation est séparée par propriétaire runtime. `MqttBridgeSession` porte la préparation du profil, la configuration broker Anycubic figée, le cycle de connexion et les abonnements dynamiques ; `MqttBridgeMessages` porte la capture redacted, le routing, les événements de fichiers imprimante, l'application au store temps réel et la corrélation des ordres HTTP/MQTT ; `MqttBridgeTelemetry` porte les buffers de diagnostic, compteurs et snapshots de télémétrie. La façade conserve uniquement la construction QObject, les propriétés/signaux publics et les petits setters d'état. CMake compile ces unités via l'ensemble unique `ACCLOUD_MQTT_BRIDGE_SOURCES`.

## Ressources QML

Les ressources production sont déclarées dans `src/accloud/app/resources.qrc`. Les pages debug sont séparées dans `resources_debug.qrc` et compilées uniquement dans les builds correspondants. Les pages, boîtes de dialogue et panes du viewer expérimental ne sont enregistrés dans aucun de ces bundles.

Une correction QML n'est valide que si le fichier est inclus dans les ressources du preset ciblé.

## Modes de build

| Preset | Build | Outils debug |
| --- | --- | --- |
| `default` | Debug | exclus |
| `dev-debug` | Debug | inclus |
| `prod` | Release | exclus |

La production ne doit jamais dépendre de `LogBridge`, `LogTailModel`, `UiClickTracer` ou de ressources QML debug-only.

## Autorité des états

```text
HTTP/cloud  = autorité de resynchronisation complète
MQTT        = autorité des transitions live
cache       = fallback explicitement étiqueté uniquement
```

Un événement MQTT obsolète ne doit pas écraser un resync HTTP plus récent. Une acceptation HTTP ne doit pas effacer un échec MQTT ultérieur.

## Thread GUI

Les opérations longues doivent être asynchrones du point de vue du thread graphique. QML ne doit pas porter les appels réseau, le parsing massif, les transactions SQLite, la génération de credentials ou les retries.

## Frontière expérimentale

Le parsing Photon/PWMB, le pipeline de jobs placeholder, le futur cache RAM/disque du viewer et `render3d` sont isolés du runtime de production. Leurs fichiers `.cpp` ne font pas partie de `accloud_infra`, ne sont pas liés à `accloud_cli`, et aucun réglage ni action viewer n'est exposé par le QML de production.

`ACCLOUD_ENABLE_EXPERIMENTAL_VIEWER` vaut `OFF` par défaut. L'opt-in explicite construit la cible objet séparée `accloud_experimental_viewer` et son smoke test de compilation via le preset `experimental-viewer-core`. Cette option ne rend pas le viewer prêt pour la production et n'enregistre aucun workflow viewer dans l'application desktop.

Voir [l'annexe viewer](annexes/viewer-photon-formats.md). Le code expérimental du viewer ne doit pas devenir une dépendance implicite des correctifs cloud, MQTT ou UI de production.
