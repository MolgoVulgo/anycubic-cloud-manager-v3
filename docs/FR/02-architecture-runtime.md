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
| `src/accloud/render3d/` | viewer PWSZ de développement fonctionnel et expérimental, exclu du runtime de production |
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

Les ressources production sont déclarées dans `src/accloud/app/resources.qrc`. Les pages debug sont séparées dans `resources_debug.qrc` et compilées uniquement dans les builds correspondants. `VolumeViewerPage.qml` et `VolumeViewerDialog.qml` sont empaquetées dans le bundle normal. Les presets desktop de développement enregistrent `Accloud.Render3D` et exposent une action 3D sur chaque ligne PWSZ. La production conserve `ACCLOUD_ENABLE_EXPERIMENTAL_VIEWER=OFF` ; l’action PWSZ reste visible mais désactivée afin que la capacité ne soit pas masquée silencieusement.

Une correction QML n'est valide que si le fichier est inclus dans les ressources du preset ciblé.

## Modes de build

| Preset | Build | Usage |
| --- | --- | --- |
| `default` | Debug + Qt | développement desktop normal ; viewer activé |
| `dev-debug` | Debug + Qt | développement desktop avec ressources et bridges debug |
| `prod` | Release + Qt | runtime production ; debug et viewer désactivés |
| `protected-core` | Debug, sans Qt | gate core portable hors ligne |
| `local-full` | Debug + Qt strict | gate local complet Qt/QML/SQL/MQTT non-live |
| `experimental-viewer-core` | Debug, sans Qt | gate isolé PWSZ/mesh/cœur viewer |
| `experimental-viewer-qt` | Debug + Qt/OpenGL strict | gate desktop complet du viewer |

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

Le parsing Photon/PWMB, les jobs/cache viewer et `render3d` restent hors de `accloud_infra`. Les presets desktop de développement les lient à `accloud_cli` via la cible isolée `accloud_experimental_viewer`; les presets `prod` et `protected-core` les excluent.

`ACCLOUD_ENABLE_EXPERIMENTAL_VIEWER` reste à `OFF` par défaut pour une configuration CMake directe. Le preset `default` l’active explicitement ; `dev-debug` et `local-full` héritent de cette valeur, tandis que `prod` et `protected-core` le désactivent explicitement. La cible objet reste nommée `accloud_experimental_viewer`. `experimental-viewer-core` construit le cœur PWSZ/mesh indépendant de Qt et `experimental-viewer-qt` reste le preset explicite de validation Qt/OpenGL. Les builds desktop activés lient le cœur à `accloud_cli`, enregistrent `Accloud.Render3D/VolumeViewer`, forcent le scene graph Qt Quick sur OpenGL et affichent un bouton 3D sur chaque ligne PWSZ. Cette action télécharge temporairement le fichier puis ouvre un dialogue de visualisation. Le chemin desktop lit et maille le PWSZ hors du thread GUI, alimente une `UploadQueue` bornée, transfère les chunks vers des buffers OpenGL et clippe le mesh sur la plage inclusive exacte choisie dans QML. Les chunks de maillage sont traités par quatre workers par défaut. Le paramètre utilisateur persistant `render3d.workerCount` accepte les valeurs de 1 à 16 et est aussi transmis à `SupportAnalyzer` lorsque l'analyse sémantique des supports est activée. `SupportAnalyzer` conserve les deux parcours sémantiques ordonnés, mais prépare les couches suivantes dans une fenêtre de workers bornée : le décodage indépendant des masques PWSZ et l'extraction des composants connexes s'exécutent en parallèle, avec au plus une couche préparée non consommée par worker. Les lectures de masques PWSZ sont concurrentes car chaque entrée d’archive est rouverte indépendamment ; pour les sources qui ne déclarent pas supporter les lectures concurrentes, seul `loadMask()` est sérialisé tandis que l'extraction indépendante des composants peut rester parallèle. Le mesher conserve le même contrat de concurrence de source. L’aperçu par défaut échantillonne une couche source sur deux tout en conservant les bornes première/dernière exactes et l’étendue Z d’origine ; l’UI permet de reconstruire avec toutes les couches. Les diagnostics structurés de génération utilisent la source `render3d` et le fichier dédié `render3d.jsonl`, avec le nombre de workers demandé/effectif et les statistiques de chaque worker. La production reste exclue dans l’attente des gates de performance et de robustesse.

Voir [l'annexe viewer](annexes/viewer-photon-formats.md). Le code expérimental du viewer ne doit pas devenir une dépendance implicite des correctifs cloud, MQTT ou UI de production.
