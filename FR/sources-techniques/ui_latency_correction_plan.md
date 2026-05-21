# Plan de correction — latence UI

Statut documentaire : `PLAN`

Source : `Docs/ui_latency_performance_analysis.md`

Date : 2026-04-30

## Objectif

Rendre l'UI interactive rapidement et supprimer les blocages perceptibles :
- interaction possible en moins de 1 s après affichage de la fenêtre ;
- changement d'onglet sans gel visible ;
- scroll fluide sur `Files` et `Recent Jobs` ;
- aucun appel réseau, SQLite, scan de logs ou gros traitement texte exécuté directement depuis QML sur le thread GUI.

Le plan est volontairement découpé en phases courtes. Chaque phase doit être validée avant de passer à la suivante afin d'éviter une régression large difficile à isoler.

## Règles d'exécution

- Ne pas mélanger deux phases dans un même changement.
- Garder les APIs synchrones existantes tant que les tests ou mocks QML en dépendent ; ajouter les APIs async en parallèle, puis migrer les appels UI.
- Après chaque phase, corriger les tests avant d'avancer.
- Ne pas refondre visuellement l'UI pendant ce chantier.
- Ne pas changer les contrats cloud/MQTT métier, uniquement le mode d'exécution et l'application des résultats côté UI.
- Documenter toute nouvelle convention async dans `Docs/02_ui_qml.md` ou dans ce plan si elle reste transitoire.

## Phase 0 — Baseline et garde-fous

### But

Mesurer le comportement actuel et figer une base de non-régression avant les refactors async.

### Changements

- Ajouter une instrumentation légère et désactivable autour des chemins coûteux :
  - `SessionImportBridge::checkStartup()`;
  - `CloudBridge::loadCachedFiles()`;
  - `CloudBridge::loadCachedPrinters()`;
  - `CloudBridge::loadCachedPrinterProjects()`;
  - `LogBridge::fetchSnapshot()`;
  - `MqttBridge::refreshTelemetrySnapshot()`.
- Utiliser `QElapsedTimer` et logguer uniquement durée, opération, nombre d'items, jamais tokens ni payloads sensibles.
- Ajouter une option de trace dédiée, par exemple variable d'environnement `ACCLOUD_UI_PERF_TRACE=1`, pour ne pas bruiter les logs par défaut.

### Tests de non-régression

Depuis `accloud/` :

```bash
cmake --build --preset default --target accloud_cli
ctest --preset default -R accloud_ui_qml --output-on-failure
ctest --preset default -R 'accloud_cache|accloud_cloud|accloud_security|accloud_mqtt_flow' --output-on-failure
```

Validation manuelle minimale :
- démarrer l'application sans la variable de trace ;
- vérifier absence de nouveaux logs perf ;
- démarrer avec `ACCLOUD_UI_PERF_TRACE=1` ;
- vérifier que les durées apparaissent sans secret.

## Phase 1 — Démarrage session async

### But

Supprimer le blocage le plus critique : `sessionImportBridge.checkStartup()` exécuté depuis `MainWindow.qml`.

### Changements

- Ajouter une méthode async à `SessionImportBridge`, par exemple `checkStartupAsync()`.
- Exécuter `CheckCloudSessionUseCase` hors thread GUI via le mécanisme async déjà utilisé dans `CloudBridge` ou une abstraction équivalente.
- Ajouter un signal de résultat, par exemple `startupCheckFinished(QVariantMap result)`.
- Modifier `MainWindow.qml` pour :
  - afficher immédiatement l'UI ;
  - lancer `checkStartupAsync()` après création ;
  - réagir au signal au lieu d'attendre un retour synchrone.
- Conserver `checkStartup()` pour compatibilité tests/mocks tant que nécessaire.

### Tests de non-régression

```bash
cmake --build --preset default --target accloud_cli
ctest --preset default -R accloud_ui_qml --output-on-failure
ctest --preset default -R accloud_security --output-on-failure
```

Scénarios manuels :
- session absente : fenêtre interactive immédiatement, dialog session après résultat ;
- session invalide : pas de freeze pendant vérification ;
- session valide : status header mis à jour après résultat.

Critère d'acceptation :
- aucun appel QML direct à `checkStartup()` dans `MainWindow.qml`.

## Phase 2 — Onglets réellement paresseux

### But

Empêcher les pages invisibles de créer bindings, timers et connexions coûteuses au démarrage.

### Changements

- Remplacer les enfants directs du `StackLayout` dans `MainWindow.qml` par des `Loader`.
- Charger `CloudFilesPage` au démarrage ou au premier affichage selon le besoin produit retenu ; charger `PrinterPage`, `MqttPage` et `LogPage` uniquement au premier accès.
- Conserver les `objectName` attendus par les tests via un host stable et/ou une propriété exposant l'item chargé.
- Ajouter une propriété `activePage` ou `pageVisible` aux pages lourdes si nécessaire.
- Suspendre les timers et traitements internes quand la page n'est pas visible :
  - `PrinterPage` auto-refresh ;
  - `MqttPage` gros bindings texte ;
  - `LogPage` polling.

### Tests de non-régression

```bash
cmake --build --preset default --target accloud_cli
ctest --preset default -R accloud_ui_qml --output-on-failure
```

Scénarios manuels :
- démarrage : seuls les composants de l'onglet initial travaillent ;
- passer à `Printers` : page chargée une seule fois ;
- revenir à `Files` : état conservé ;
- ouvrir `MQTT` puis revenir : pas de traitement raw stream hors onglet actif.

Critère d'acceptation :
- `MqttPage` et `LogPage` ne doivent pas être instanciées avant ouverture de leur onglet.

## Phase 3 — Cache async et lecture groupée imprimantes

### But

Supprimer les lectures SQLite synchrones depuis le thread UI, en priorité sur `Printers`.

### Changements

- Ajouter des méthodes async dans `CloudBridge` :
  - `loadCachedFilesAsync(page, limit)`;
  - `loadCachedQuotaAsync()`;
  - `loadCachedPrintersAsync()`;
  - `loadCachedPrinterProjectsAsync(printerId, page, limit)`.
- Ajouter les signaux correspondants :
  - `cachedFilesLoaded(...)`;
  - `cachedQuotaLoaded(...)`;
  - `cachedPrintersLoaded(...)`;
  - `cachedPrinterProjectsLoaded(...)`.
- Migrer `CloudFilesPage` et `PrinterPage` vers ces méthodes async.
- Conserver les méthodes synchrones pour tests existants, puis adapter les tests QML progressivement.
- Corriger le N+1 de `loadCachedPrinters()` :
  - créer une lecture groupée imprimantes + derniers jobs par imprimante ;
  - éviter `loadJobsForPrinter()` dans une boucle côté UI path ;
  - réduire les ouvertures/suppressions de connexions SQLite.
- Garder le verrou SQLite hors thread UI.

### Tests de non-régression

```bash
cmake --build --preset default --target accloud_cli
ctest --preset default -R accloud_ui_qml --output-on-failure
ctest --preset default -R accloud_cache --output-on-failure
ctest --preset default -R accloud_cloud --output-on-failure
```

Scénarios manuels :
- onglet `Files` : affichage rapide avec état loading, puis remplissage cache ;
- onglet `Printers` : changement d'onglet immédiat, puis données cache ;
- cache vide : messages existants conservés ;
- cache présent + refresh cloud : pas de double reconstruction visible.

Critère d'acceptation :
- aucun appel QML direct à `loadCachedFiles`, `loadCachedQuota`, `loadCachedPrinters`, `loadCachedPrinterProjects`.

## Phase 4 — Actions cloud non bloquantes

### But

Supprimer les blocages utilisateur lors des actions : détails, download URL, delete, compatibilité, remote print.

### Changements

- Ajouter des variantes async dans `CloudBridge` pour les actions encore synchrones :
  - `getDownloadUrlAsync(fileId)`;
  - `deleteFileAsync(fileId)`;
  - `fetchCompatiblePrintersByExtAsync(fileExt)`;
  - `fetchCompatiblePrintersByFileIdAsync(fileId)`;
  - `fetchReasonCatalogAsync()` si le fallback synchrone reste utilisé ;
  - `sendPrintOrderAsync(...)`;
  - `sendPrinterOrderAsync(...)`.
- Donner à chaque opération un identifiant ou un contexte minimal pour éviter d'appliquer un résultat périmé.
- Adapter `CloudFilesPage` et `PrinterPage` :
  - états loading par opération ;
  - boutons désactivés pendant l'opération ciblée ;
  - résultat appliqué uniquement si le contexte courant correspond encore.
- Supprimer les fallbacks synchrones côté QML sauf dans les mocks de tests.

### Tests de non-régression

```bash
cmake --build --preset default --target accloud_cli
ctest --preset default -R accloud_ui_qml --output-on-failure
ctest --preset default -R 'accloud_cloud|accloud_mqtt_flow' --output-on-failure
```

Scénarios manuels :
- suppression fichier : UI reste scrollable pendant l'appel ;
- récupération URL download : pas de freeze avant ouverture du save dialog ;
- remote print : dialog reste responsive pendant `sendPrintOrder`;
- erreurs backend : messages existants conservés.

Critère d'acceptation :
- aucun workflow utilisateur réseau ne doit attendre un retour direct dans QML.

## Phase 5 — MQTT et logs bornés par visibilité

### But

Empêcher MQTT et logs de réveiller ou bloquer l'UI quand leurs onglets sont invisibles.

### Changements MQTT

- Ne plus binder `MqttPage` directement à `mqttBridge.rawBuffer`.
- Exposer un modèle ou un snapshot borné :
  - tail de messages limité ;
  - messages par topic paginés ;
  - refresh UI déclenché seulement si l'onglet MQTT est actif.
- Remplacer `filteredRawStream()` sur 200000 caractères par un filtre sur modèle borné.
- Garder `rawBuffer` pour debug bas niveau si nécessaire, mais pas comme source principale de `TextArea`.

### Changements logs

- Charger `LogPage` uniquement quand l'onglet est visible.
- Désactiver le timer de polling si l'onglet logs n'est pas actif.
- Modifier `LogBridge::fetchSnapshot()` ou ajouter `fetchTailSnapshot()` :
  - lire uniquement les dernières lignes utiles ;
  - éviter parse + tri de tout l'historique à chaque seconde.

### Tests de non-régression

```bash
cmake --build --preset default --target accloud_cli
ctest --preset default -R accloud_ui_qml --output-on-failure
ctest --preset default -R accloud_mqtt_flow --output-on-failure
```

Scénarios manuels :
- MQTT actif mais onglet non ouvert : pas de saccade sur `Files`/`Printers`;
- onglet MQTT ouvert : messages visibles et filtrables ;
- build debug avec logs : polling uniquement onglet actif ;
- retour sur autre onglet : polling suspendu.

Critère d'acceptation :
- aucun gros buffer texte ne doit être recalculé chaque seconde hors page active.

## Phase 6 — Modèles C++ incrémentaux

### But

Remplacer progressivement les `ListModel` QML lourds par des modèles C++ incrémentaux.

### Changements

- Introduire des `QAbstractListModel` ciblés :
  - fichiers cloud ;
  - imprimantes ;
  - jobs récents ;
  - tail MQTT ;
  - tail logs si utile.
- Exposer les rôles déjà pré-formatés nécessaires à QML :
  - textes affichés ;
  - couleurs/statuts si purement présentation stable ;
  - identifiants métier.
- Appliquer des deltas :
  - `dataChanged` pour mise à jour ligne ;
  - `beginInsertRows/endInsertRows`;
  - `beginRemoveRows/endRemoveRows`;
  - éviter `clear()` sauf changement complet nécessaire.
- Migrer page par page :
  1. jobs récents ;
  2. imprimantes ;
  3. fichiers ;
  4. MQTT/logs.

### Tests de non-régression

Après chaque modèle migré :

```bash
cmake --build --preset default --target accloud_cli
ctest --preset default -R accloud_ui_qml --output-on-failure
```

Après la phase complète :

```bash
ctest --preset default --output-on-failure
```

Scénarios manuels :
- sélection conservée après refresh ;
- pagination/filtre fichiers conservés ;
- jobs récents enrichis sans effacement ;
- MQTT/logs restent bornés.

Critère d'acceptation :
- les listes principales ne doivent plus dépendre de reconstructions `QVariantList` complètes côté QML.

## Phase 7 — Nettoyage, simplification et documentation

### But

Réduire la dette introduite par les APIs transitoires et verrouiller les nouvelles règles.

### Changements

- Supprimer les fallbacks synchrones QML devenus inutiles.
- Garder les méthodes synchrones C++ seulement si elles servent CLI/tests unitaires, pas l'UI runtime.
- Découper `PrinterPage.qml` en sous-composants si le comportement est stabilisé :
  - état/listing ;
  - détails ;
  - remote print ;
  - fichiers locaux ;
  - jobs récents.
- Mettre à jour :
  - `Docs/02_ui_qml.md`;
  - `Docs/ui_latency_performance_analysis.md` si les conclusions changent ;
  - ce plan avec le statut réel des phases exécutées.

### Tests de non-régression

```bash
cmake --build --preset default --target accloud_cli
ctest --preset default --output-on-failure
```

Validation manuelle finale :
- démarrage avec session valide ;
- démarrage sans session ;
- passage `Files` -> `Printers` -> `MQTT` -> `Files`;
- scroll long `Files`;
- scroll `Recent Jobs`;
- MQTT actif pendant navigation ;
- build debug avec logs.

## Ordre recommandé des commits

1. `docs(ui): add latency correction plan`
2. `perf(ui): add latency instrumentation`
3. `perf(app): make startup session check async`
4. `perf(ui): lazy-load heavy tabs`
5. `perf(cache): load cached printer dashboard asynchronously`
6. `perf(ui): consume cache load results asynchronously`
7. `perf(cloud): make user cloud actions asynchronous`
8. `perf(mqtt): bound runtime stream updates to active page`
9. `perf(logs): tail logs without full history scan`
10. `perf(ui): migrate hot lists to incremental models`
11. `refactor(ui): split printer page responsibilities`

## Risques et points de vigilance

- Les tests QML actuels mockent des retours synchrones ; il faudra probablement adapter les mocks pour émettre les nouveaux signaux async.
- Les phases 1 à 4 doivent conserver temporairement les anciennes méthodes pour limiter le risque.
- Les signaux async doivent transporter assez de contexte pour éviter d'appliquer un résultat ancien sur une sélection récente.
- Le cache SQLite est partagé avec les refresh de fond ; le déplacement hors thread UI ne suffit pas si le mutex global garde des sections longues.
- Les optimisations de delegates ne doivent pas masquer le problème principal : l'I/O et le réseau ne doivent plus bloquer le thread GUI.

## Critère final de réussite

Le chantier est considéré terminé quand :
- l'UI reste interactive pendant startup, refresh cloud, lecture cache et MQTT actif ;
- le passage d'onglet ne déclenche plus de freeze perceptible ;
- les listes scrollent sans blocage durable ;
- `ctest --preset default --output-on-failure` passe ;
- le QML Profiler ne montre plus de longues plages bloquées par appels C++ synchrones depuis QML.
