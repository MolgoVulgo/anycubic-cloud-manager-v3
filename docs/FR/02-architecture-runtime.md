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

`ACCLOUD_ENABLE_EXPERIMENTAL_VIEWER` reste à `OFF` par défaut pour une configuration CMake directe. Le preset `default` l’active explicitement ; `dev-debug` et `local-full` héritent de cette valeur, tandis que `prod` et `protected-core` le désactivent explicitement. La cible objet reste nommée `accloud_experimental_viewer`. `experimental-viewer-core` construit le cœur PWSZ/mesh indépendant de Qt et `experimental-viewer-qt` reste le preset explicite de validation Qt/OpenGL. Les builds desktop activés lient le cœur à `accloud_cli`, enregistrent `Accloud.Render3D/VolumeViewer`, forcent le scene graph Qt Quick sur OpenGL et affichent un bouton 3D sur chaque ligne PWSZ. Cette action télécharge temporairement le fichier puis ouvre un dialogue de visualisation. Le chemin desktop lit et maille le PWSZ hors du thread GUI, alimente une `UploadQueue` bornée, transfère les chunks vers des buffers OpenGL et clippe le mesh sur la plage inclusive exacte choisie dans QML. Les chunks de maillage sont traités par quatre workers par défaut. Le paramètre utilisateur persistant `render3d.workerCount` accepte les valeurs de 1 à 16 et est aussi transmis à `SupportAnalyzer` lorsque l'analyse sémantique des supports est activée. `SupportAnalyzer` utilise un unique ordonnanceur persistant à priorité pour la préparation des couches natives et les travaux sémantiques indépendants. P6.3 supprime l'ancien plafond fixe de quatre masques : la fenêtre de préparation devient le minimum entre le nombre de workers configuré, les couches restantes et un budget mémoire de masques natifs (256 Mio par défaut), ce qui permet à une station à 16 workers de préparer jusqu'à 16 couches en parallèle lorsque la résolution tient dans ce budget. Les lectures de masques PWSZ sont concurrentes car chaque entrée d’archive est rouverte indépendamment ; pour les sources qui ne déclarent pas supporter les lectures concurrentes, seul `loadMask()` est sérialisé tandis que l'ordonnanceur partagé continue de paralléliser l'extraction indépendante des composants. P6.5 sépare la géométrie immuable de la réconciliation sémantique : chaque `LayerDescription` native est préparée une seule fois, puis toutes les paires de couches sémantiques adjacentes sont réparties en lots contigus et leur graphe clairsemé de preuves géométriques (recouvrement, distance matière et distance des centres) est construit en parallèle. Les commits sémantiques montant et descendant réconcilient encore ce graphe dans un ordre déterministe de couches/composants ; P6.5 constitue donc la première étape graphe/lots et non un classifieur spéculatif indépendant. Les frontières entre lots sont représentées explicitement par les arêtes entre couches adjacentes et ne nécessitent pas de halo dupliqué à cette étape. P5 conserve les runs clairsemés comme représentation autoritaire tout en ajoutant une accélération AVX2/bitset optionnelle avec fallback scalaire. P6/P6.1/P6.2 ajoute un backend Vulkan Compute optionnel dédié au comptage des recouvrements translatés utilisés par la recherche de lignée pièce. Il n'est construit que lorsqu'un SDK Vulkan et un compilateur SPIR-V sont disponibles, reste indépendant du renderer OpenGL et laisse l'ordre des couches, les mutations du graphe, la réconciliation et les commits déterministes sur le CPU. P6.1 regroupe les requêtes concurrentes des gros composants via un dispatcher Vulkan dédié et utilise des buffers staging persistants ainsi que des buffers de stockage device-local réutilisables au lieu de sérialiser chaque worker derrière un mutex GPU par composant. P6.2 augmente l'occupation GPU native avec un dispatch 3D (`tuile × translation × job`) : les jobs denses découpent le domaine de mots par tuiles de 4096 mots, tandis que le chemin principal de lignée sur le modèle stable envoie les runs sémantiques compacts par tuiles de 64 runs. La référence du modèle stable est rasterisée une seule fois par couche dans le domaine raster natif et peut rester résidente dans un buffer Vulkan device-local réutilisable pour tous les composants compatibles de cette couche, évitant un bitmap source dense et le renvoi de la référence pour chaque composant. Le runtime expose seulement deux modes compute standard : `auto` (hybride CPU/Vulkan) et `cpu`. `auto` retombe sur le chemin CPU canonique si Vulkan ne peut pas être initialisé ou si un job GPU échoue ; il n'existe plus de mode runtime full GPU. Les diagnostics runtime exposent dès le début de l'analyse le périphérique sélectionné, les jobs GPU/fallback et runs compacts, les uploads/réutilisations de référence résidente, les workgroups soumis, la taille des lots, le volume transféré et les temps hôte/file/exécution. P6.3 publie aussi en continu les temps de préparation/sémantique (`support_prepare_load_us`, `support_prepare_describe_us`, `support_forward_semantic_us`, `support_reverse_semantic_us`) ainsi que le nombre de couches préparées, la capacité de fenêtre et le maximum en vol, afin qu'une analyse annulée tôt indique déjà si le coût dominant vient de la préparation ou des passes sémantiques ordonnées. Le mesher conserve le même contrat de concurrence de source. L’aperçu par défaut échantillonne une couche source sur deux tout en conservant les bornes première/dernière exactes et l’étendue Z d’origine. Les diagnostics structurés de génération utilisent la source `render3d` et le fichier dédié `render3d.jsonl`, avec le nombre de workers demandé/effectif et la télémétrie d'activation/dispatch Vulkan de l'analyse des supports. La production reste exclue dans l’attente des gates de performance et de robustesse. P6.4 a introduit le regroupement Vulkan des recouvrements à translation nulle au niveau couche et a indexé plusieurs scans sémantiques sériels. Le benchmark du gros fichier a ensuite montré que ce regroupement régressait le mode `auto` : des millions de requêtes hôte par composant augmentaient le coût de file/synchronisation sans assez de travail GPU utile. La stabilisation qui suit P6.5 retire entièrement ce lot couche P6.4 du runtime de l'analyseur : `auto` et `cpu` utilisent tous deux le chemin CPU clairsemé canonique pour les comptages exacts/enveloppes à translation nulle, tandis que `auto` peut utiliser Vulkan uniquement pour les kernels de lignée translatée. La primitive Vulkan bulk bas niveau reste couverte par son test backend mais n'est plus un mode runtime sélectionnable. P6.5 dimensionne aussi les buffers de comptage d'enfants, dérive de branche et contact pièce depuis les seuls candidats de la couche précédente au lieu de `states.size()` cumulatif, supprimant les remises à zéro répétées proportionnelles à tout l'historique du graphe. Les temps par sous-phase P6.4 restent disponibles et P6.5 ajoute `support_semantic_evidence_us`, `support_semantic_evidence_lots`, `support_semantic_evidence_layer_pairs` et `support_semantic_evidence_edges` pour mesurer la construction parallèle du graphe.

Voir [l'annexe viewer](annexes/viewer-photon-formats.md). Le code expérimental du viewer ne doit pas devenir une dépendance implicite des correctifs cloud, MQTT ou UI de production.
