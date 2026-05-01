# Continuite optimisation UI

Statut documentaire : `PLAN`

Date : 2026-05-01

Source :
- `Docs/ui_latency_performance_analysis.md`
- `Docs/ui_latency_correction_plan.md`
- code actuel apres commits `848dba7` et `61b0c5a`

## Objectif

Garder la continuite du chantier d'optimisation sans recreer de blocages UI.
La regle directrice est stricte : aucune operation reseau, SQLite, scan de logs
ou traitement de gros buffer texte ne doit etre declenchee directement par QML
sur le thread GUI.

## Etat acquis

- Le controle session de demarrage passe par `checkStartupAsync()` et emet
  `startupCheckFinished`.
- `CloudFilesPage` utilise le chargement async du cache fichiers/quota quand
  le bridge le fournit.
- `PrinterPage` utilise `loadCachedPrintersAsync()` pour le chargement initial
  cache imprimantes.
- Les commandes locales imprimante passent par `sendPrinterOrderAsync()` quand
  disponible.
- `loadCachedPrinters()` evite le N+1 jobs via
  `LocalCacheStore::loadRecentJobsForPrinters`.
- `MqttPage` n'est chargee que lorsque l'onglet MQTT est actif.
- `CloudFilesModel::append()` et `MqttTailModel::appendMessage()` evitent des
  resets complets dans les cas courants.
- Le test `accloud_ui_models` couvre les contrats de base des modeles UI.

## Regles a conserver

- Garder les APIs synchrones existantes uniquement comme compatibilite, tests
  ou fallback local ; ne pas les appeler depuis les nouveaux chemins QML.
- Toute nouvelle API async doit emettre un signal de resultat avec assez de
  contexte pour ignorer un resultat perime.
- Un signal qui transporte une liste complete doit rester borne ou etre remplace
  par un modele incremental si le volume peut augmenter.
- Les pages invisibles ne doivent pas conserver de timers, polling ou bindings
  sur gros buffers.
- Ne jamais logger tokens, credentials, donnees de session ou payloads MQTT
  sensibles pendant les mesures de performance.

## Prochaine marche a suivre

### Phase A - Fermer les fallbacks synchrones QML

Objectif : supprimer les derniers appels directs QML vers des methodes pouvant
faire reseau ou disque.

Verifier et migrer :
- `fetchFiles`, `fetchPrinters`, `fetchPrinterProjects`, `fetchPrinterDetails`
  vers signaux async ou cache async ;
- `fetchCompatiblePrintersByExt` et `fetchCompatiblePrintersByFileId` vers une
  variante async si le chemin UI les appelle hors mock ;
- `fetchReasonCatalog` vers le refresh async existant ;
- `loadCachedPrinterProjects` vers `loadCachedPrinterProjectsAsync`.

Tests apres phase :

```bash
cmake --build --preset default --target accloud_cli
ctest --preset default -R 'accloud_ui_qml|accloud_ui_models|accloud_cloud_core_regressions' --output-on-failure
```

### Phase B - Suspendre les flux hors onglet actif

Objectif : empecher MQTT, logs et auto-refresh imprimantes de reveiller l'UI
quand la page n'est pas visible.

Actions :
- ajouter/propager une propriete `pageActive` aux pages lourdes restantes ;
- suspendre les timers de `PrinterPage` quand l'onglet `Printers` est inactif ;
- verifier que `LogPage` ne poll pas hors onglet actif en build debug ;
- limiter les snapshots MQTT aux vues visibles.

Tests apres phase :

```bash
cmake --build --preset default --target accloud_cli
ctest --preset default -R 'accloud_ui_qml|accloud_log_flow|accloud_mqtt_flow' --output-on-failure
```

Validation manuelle :
- ouvrir `Files`, laisser MQTT connecte, verifier absence de saccade notable ;
- ouvrir `Printers`, attendre un cycle auto-refresh, revenir `Files` ;
- en build debug, ouvrir puis quitter `Logs`, verifier que le polling cesse.

### Phase C - Remplacer les gros `ListModel` QML

Objectif : reduire les conversions `QVariantList` et les reconstructions
completes.

Priorite :
1. modele C++ pour fichiers cloud de selection remote print ;
2. modele C++ pour fichiers locaux imprimante ;
3. modele C++ ou store dedie pour details/jobs dans `PrinterPage`.

Regle : chaque modele doit exposer des deltas (`rowsInserted`,
`dataChanged`, reset seulement quand l'identite ou le tri change).

Tests apres phase :

```bash
cmake --build --preset default --target accloud_ui_model_tests
ctest --preset default -R 'accloud_ui_models|accloud_ui_qml' --output-on-failure
```

### Phase D - Borner logs et buffers texte

Objectif : supprimer les operations O(historique complet) dans les vues debug.

Actions :
- ajouter une lecture tail bornee dans `LogBridge` ;
- ne plus reconstruire un texte geant via `join("\n")` si un modele peut
  alimenter la vue ;
- garder `rawBuffer` MQTT comme outil debug bas niveau, pas comme source
  principale de rendu.

Tests apres phase :

```bash
cmake --build --preset default --target accloud_cli
ctest --preset default -R 'accloud_log_flow|accloud_mqtt_flow|accloud_ui_qml' --output-on-failure
```

### Phase E - Mesurer avant nouvelle micro-optimisation

Objectif : eviter les changements cosmetiques non mesures.

Mesurer :
- temps jusqu'a premiere interaction ;
- premier passage `Files -> Printers` ;
- ouverture `MQTT` avec flux actif ;
- ouverture `Logs` en build `dev-debug` ;
- scroll `Files` et `Recent Jobs`.

Utiliser les traces existantes (`UiPerfTrace`) et ajouter des compteurs bornes
uniquement si une phase en a besoin.

## Definition de termine

Une phase est terminee seulement si :
- les appels QML synchrones vises ont disparu ou sont justifies comme mock/test ;
- les tests indiques passent ;
- le comportement utilisateur degrade est couvert par un signal d'erreur ou un
  etat loading ;
- la doc impactee est mise a jour ;
- aucun secret ou payload sensible n'est ajoute aux logs.

## Commande de validation rapide

Pour une passe non-live apres une phase UI :

```bash
cmake --build --preset default --target accloud_cli
ctest --preset default -E 'live' --output-on-failure
```

Les tests live MQTT restent separes : ils exigent broker, TLS et session
disponibles.
