# Analyse latence UI / workflows applicatifs

Statut documentaire : `ANALYSE`

Date : 2026-04-30

## Résumé exécutif

Le symptôme décrit, 1 à 10 s avant interaction possible, onglets à 1-2 s et listes qui bloquent, ne correspond pas seulement à un problème de delegates QML. Le problème principal est architectural : l'UI exécute encore trop de travail synchrone sur le thread GUI.

Les optimisations locales des listes réduisent une partie du coût de rendu, mais elles ne peuvent pas corriger :
- les appels réseau synchrones depuis QML ;
- les lectures SQLite synchrones depuis QML ;
- les reconstructions de modèles à partir de gros `QVariantList` ;
- les bindings actifs de pages instanciées mais invisibles ;
- les flux MQTT/logs qui réveillent l'UI périodiquement.

Priorité de correction : sortir tous les workflows I/O et réseau du thread UI, puis rendre les onglets et flux lourds réellement paresseux.

## Workflow de démarrage

### Chemin actuel

`MainWindow.qml` instancie la shell principale, puis `StackLayout` crée directement les pages :
- `CloudFilesPage`
- `PrinterPage`
- `MqttPage`
- host de logs

`CloudFilesPage` lance `loadFiles()` dans `Component.onCompleted`. Ce chemin appelle synchroniquement :
- `cloudBridge.loadCachedQuota()`
- `cloudBridge.loadCachedFiles(1, 20)`

Ensuite, `MainWindow.qml` appelle `sessionImportBridge.checkStartup()` via `Qt.callLater`. Cette fonction C++ charge la session puis exécute `CheckCloudSessionUseCase`. Ce contrôle cloud est fait dans un `Q_INVOKABLE` synchrone, donc sur le thread GUI.

### Impact

C'est le candidat le plus fort pour expliquer les 1 à 10 s avant UI interactive. Même avec `Qt.callLater`, l'appel reste exécuté par le thread UI. Si le cloud, DNS, TLS ou disque est lent, l'UI ne peut pas traiter input, animations ou rendu.

### Conclusion

Le check startup doit devenir asynchrone. Il doit émettre un signal de résultat, comme les refresh cloud existants, au lieu de retourner un `QVariantMap` bloquant.

## Passage d'onglets

### Chemin actuel

Le passage à `Printers` déclenche `printerPage.ensureStartupInitialized()`. Ce chemin appelle :
- `loadPrinters()`
- `cloudBridge.loadCachedPrinters()`
- `refreshSelectedPrinterJobs("startup", true, true)`
- `cloudBridge.loadCachedPrinterProjects(selectedPrinterId, 1, 20)`
- `cloudBridge.refreshPrinterInsightsAsync(...)`

Même si le refresh cloud est async, les lectures cache sont synchrones.

### Coût identifié

`loadCachedPrinters()` appelle `LocalCacheStore::loadPrinters()`, puis pour chaque imprimante appelle `loadJobsForPrinter(printerId, 1, 20)`. C'est un modèle N+1 :
- 1 requête SQLite pour les imprimantes ;
- N requêtes SQLite pour les jobs ;
- ouverture/suppression de connexions SQLite ;
- verrou global `g_dbMutex`.

Si une tâche de fond écrit dans le cache au même moment, le thread UI peut attendre le mutex.

### Conclusion

Le passage onglet `Printers` peut légitimement bloquer 1-2 s si le cache est chargé, verrouillé, ou si le disque est lent. La correction locale QML ne suffit pas.

## Workflow Files

### Chemin actuel

Au démarrage de la page :
- quota cache synchrone ;
- fichiers cache synchrones ;
- reconstruction QML du modèle visible ;
- refresh cloud async forcé.

Sur actions utilisateur :
- `getDownloadUrl()` est synchrone ;
- `deleteFile()` est synchrone ;
- fallback `uploadLocalFile()` est synchrone ;
- `fetchFiles()` fallback est synchrone.

### Poids des demandes

`loadCachedFiles(1, 20)` est limité, donc moins lourd que les imprimantes. Le problème apparaît surtout quand :
- l'appel prend le verrou SQLite global ;
- le refresh async renvoie une liste complète et réveille QML ;
- des thumbnails sont décodés ou changent de statut pendant le scroll.

### Conclusion

Le listing `Files` a été allégé, mais les appels synchrones et les images restent des sources de frame drop. Les actions fichier doivent aussi migrer vers des API async.

## Workflow Printers / Tasks

### Chemin actuel

La page `PrinterPage.qml` est très lourde : environ 2650 lignes, beaucoup d'état local, plusieurs `ListModel`, plusieurs workflows cloud/MQTT/compatibilité.

La page combine :
- liste imprimantes ;
- détails imprimante ;
- job actif ;
- historique jobs ;
- compatibilité fichiers ;
- commandes remote print ;
- commandes MQTT de fichiers locaux ;
- auto-refresh.

### Refresh périodique

`PrinterPage` active un timer :
- 30 s par défaut ;
- 5 s si une imprimante imprime.

À chaque tick, `refreshPrintersAsync(false)` peut revenir avec un `QVariantList` complet, puis QML compare/met à jour les modèles. Même optimisé, le signal et la conversion QVariant vers QML restent sur le thread UI au moment de l'application.

### Conclusion

La page mélange trop de responsabilités et reçoit trop de stimuli. Même si chaque opération est moyenne, leur somme crée une UI instable sous charge.

## Workflow MQTT

### Chemin actuel

`MqttBridge` installe :
- un timer télémétrie à 1000 ms ;
- un timer refresh subscriptions à 30000 ms ;
- des signaux `rawBufferChanged`, `messageTickChanged`, `realtimeEventTickChanged`, `telemetrySnapshotChanged`.

`MqttPage` est instanciée au démarrage par le `StackLayout`. Elle contient des bindings sur :
- `mqttBridge.rawBuffer`
- `mqttBridge.telemetrySnapshot`
- `mqttBridge.subscribedTopics`
- `mqttBridge.messagesForTopic(...)`

`filteredRawStream()` split le buffer brut ligne par ligne. Le buffer peut atteindre 200000 caractères.

### Impact

Même si l'onglet MQTT n'est pas visible, l'objet QML existe. Les bindings et connexions peuvent donc être réveillés par les signaux MQTT. Un flux MQTT actif peut dégrader la fluidité globale.

### Conclusion

MQTT doit être découplé de l'UI visible :
- ne pas instancier `MqttPage` avant ouverture ;
- ne pas binder directement les gros buffers ;
- pousser des snapshots limités uniquement quand l'onglet est actif ;
- remplacer le texte brut complet par un modèle paginé/tail.

## Workflow Logs

### Chemin actuel

En build debug, `LogPage` est chargée automatiquement. Elle lance un timer 1 s et appelle `logBackend.fetchSnapshot(root.maxTailLines)`.

`LogBridge::fetchSnapshot()` :
- lit tous les fichiers `*.jsonl` ;
- parse toutes les lignes ;
- trie les entrées ;
- tronque ensuite à `maxLines`.

Ensuite QML refiltre toutes les entrées et reconstruit un gros texte via `join("\n")`.

### Impact

En build debug, cette page peut consommer fortement le thread UI toutes les secondes, même sans interaction avec les logs.

### Conclusion

La page logs doit être inactive tant que l'onglet n'est pas visible et le backend doit lire directement un tail borné, pas tout l'historique.

## Rendu QML et structure visuelle

### Pages instanciées

`StackLayout` conserve toutes les pages en mémoire. Les pages invisibles ne sont pas rendues, mais leurs objets, bindings, timers et `Connections` restent actifs.

### Delegates

Les listes ont déjà été partiellement optimisées :
- modèle visible pour `Files` ;
- delegates jobs plus simples ;
- cacheBuffer ;
- mises à jour différentielles partielles.

Mais il reste :
- beaucoup de logique JS dans `PrinterPage`;
- conversions `QVariantMap`/`QVariantList`;
- images async mais toujours décodées et suivies par bindings ;
- status global mis à jour par plusieurs pages ;
- composants de tabs avec `Canvas` qui repeignent sur hover/check.

### Conclusion

Le rendu seul n'explique pas 10 s, mais il explique le scroll irrégulier lorsque le thread UI est déjà occupé par I/O, signaux ou gros bindings.

## Poids relatif des demandes

| Demande / workflow | Poids estimé | Risque UI |
| --- | --- | --- |
| `sessionImportBridge.checkStartup()` | réseau + session | Très élevé, synchrone |
| `cloudBridge.fetch*()` fallback | réseau | Très élevé, synchrone |
| `cloudBridge.loadCachedPrinters()` | SQLite + N+1 jobs + mutex | Élevé, synchrone |
| `cloudBridge.loadCachedFiles()` | SQLite limité à 20 | Moyen, synchrone |
| `cloudBridge.loadCachedPrinterProjects()` | SQLite jobs | Moyen à élevé, synchrone |
| `refreshPrintersAsync()` résultat | QVariantList complet vers QML | Moyen |
| `refreshFilesAsync()` résultat | QVariantList + thumbnails | Moyen |
| `MqttPage.filteredRawStream()` | O(taille rawBuffer) | Élevé si MQTT actif |
| `LogBridge.fetchSnapshot()` | O(toutes lignes logs) + tri | Très élevé en debug |
| thumbnails / previews | décodage image | Moyen |

## Causes racines probables

1. Appels réseau synchrones depuis le thread UI, surtout `checkStartup()` et les fallbacks `fetch*()`.
2. Lectures cache synchrones depuis QML, avec SQLite sous mutex global.
3. N+1 queries dans le chargement imprimantes + jobs.
4. `StackLayout` qui instancie les pages lourdes même invisibles.
5. MQTT et logs qui peuvent réveiller l'UI périodiquement.
6. Gros buffers texte (`rawBuffer`, logs) transformés en `TextArea`.
7. Modèles QML construits depuis `QVariantList` plutôt que vrais modèles C++ incrémentaux.
8. Trop de responsabilités dans `PrinterPage.qml`, donc trop de bindings et d'état couplé.

## Priorités de correction recommandées

### P0 - Débloquer le thread UI

- Transformer `SessionImportBridge::checkStartup()` en workflow async.
- Ne plus appeler `fetchFiles`, `fetchPrinters`, `fetchPrinterDetails`, `fetchPrinterProjects`, `fetchReasonCatalog`, `sendPrintOrder`, `sendPrinterOrder`, `deleteFile`, `getDownloadUrl` directement depuis QML si l'opération peut faire réseau ou disque.
- Ajouter des signaux de résultat et des états loading par opération.

### P1 - Rendre les onglets réellement paresseux

- Remplacer les pages du `StackLayout` par des `Loader`.
- Activer une page seulement au premier affichage.
- Désactiver ou suspendre les timers/connections des pages non visibles.
- Charger `MqttPage` uniquement quand l'onglet MQTT est ouvert.
- Charger `LogPage` uniquement quand l'onglet Logs est ouvert.

### P1 - Corriger le cache imprimantes

- Remplacer le N+1 `loadPrinters()` + `loadJobsForPrinter()` par une lecture cache groupée.
- Éviter ouverture/suppression de connexion SQLite à chaque petite lecture.
- Réduire la durée du verrou global `g_dbMutex`.
- Ne jamais attendre ce verrou depuis le thread UI.

### P2 - Modèles de données

- Remplacer les `ListModel` QML principaux par des `QAbstractListModel` C++ :
  - fichiers ;
  - imprimantes ;
  - jobs récents ;
  - MQTT tail ;
  - logs tail.
- Appliquer des deltas (`dataChanged`, `rowsInserted`, `rowsRemoved`) au lieu de `clear/append`.
- Garder les formats affichés pré-calculés côté modèle.

### P2 - Flux texte et images

- MQTT : exposer un tail borné ou une liste paginée, jamais un `rawBuffer` complet bindé à un `TextArea`.
- Logs : lire seulement les dernières lignes utiles, pas tous les fichiers.
- Images : limiter dimensions source/cache, éviter les validations `QImageReader` répétées dans les workflows UI, ne charger les previews que si visibles.

## Plan de diagnostic recommandé

Avant une nouvelle optimisation à l'aveugle :

1. Lancer l'application avec QML Profiler.
2. Mesurer séparément :
   - startup jusqu'à première frame interactive ;
   - premier passage à `Printers`;
   - scroll `Files`;
   - scroll `Recent Jobs`;
   - MQTT actif vs MQTT désactivé ;
   - build default vs dev-debug.
3. Ajouter temporairement des mesures `QElapsedTimer` autour :
   - `checkStartup()`;
   - `loadCachedFiles()`;
   - `loadCachedPrinters()`;
   - `loadCachedPrinterProjects()`;
   - `LogBridge::fetchSnapshot()`;
   - `MqttBridge::refreshTelemetrySnapshot()`.
4. Vérifier les warnings Qt en runtime. Les warnings répétés passent par `qtMessageHandler` et peuvent provoquer de l'I/O log synchrone.

## Conclusion finale

Le problème cible n'est pas prioritairement un problème de style QML ou de hauteur de delegate. La latence générale vient d'un mélange de travaux synchrones, pages pré-instanciées, flux périodiques et modèles QVariant lourds.

La correction durable doit commencer par une règle stricte : aucune opération réseau, SQLite, log scan ou gros buffer texte ne doit être appelée directement par QML sur le thread GUI. Ensuite seulement les optimisations de delegates et de rendu produiront un effet visible.
