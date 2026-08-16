# AGENTS.md — Anycubic Cloud Manager V3

Ce fichier définit les contraintes projet chargées avant toute intervention.

## Cadre

Le dépôt est un projet C++20 / Qt6 / QML construit avec CMake. Python est réservé au tooling et aux contrôles annexes. Il ne doit pas devenir la référence architecturale de l'application.

## Ordre de lecture

Les règles de production et de livraison des correctifs sont fournies par l’environnement GPT Web. Elles ne font pas partie du dépôt et ne doivent pas être recherchées, copiées ou recréées localement.

1. `codex-patch-mode.md` uniquement lorsqu’un patch ZIP déjà produit doit être appliqué mécaniquement ;
2. `accloud/CMakeLists.txt` et `accloud/CMakePresets.json` ;
3. `docs/README.md` ou `docs/FR/README.md` ;
4. le document fonctionnel correspondant à la zone réellement touchée ;
5. le code actif et les tests déclarés par CMake.

Les documents sous `docs/FR/sources-techniques/` sont des matériaux historiques ou d'investigation. Ils ne remplacent jamais la documentation principale ni le runtime actif.

## Points d'entrée actifs

- bootstrap : `src/accloud/app/main.cpp` ;
- ressources QML : `src/accloud/app/resources.qrc` ;
- ressources debug : `src/accloud/app/resources_debug.qrc` ;
- fenêtre principale : `src/accloud/ui/qml/MainWindow.qml` ;
- cloud : `src/accloud/infra/cloud/` ;
- MQTT : `src/accloud/infra/mqtt/`, façade `src/accloud/app/MqttBridge.cpp` et cycle session/abonnements `src/accloud/app/mqtt/MqttBridgeSession.cpp` ;
- viewer PWSZ expérimental : `src/accloud/render3d/`, `src/accloud/ui/qml/pages/VolumeViewerPage.qml` et `VolumeViewerDialog.qml` ;
- analyse des supports : `src/accloud/render3d/analysis/SupportAnalyzer.*`, avec diagnostic debug via `SupportAnalysisBridge`.

## Invariants

- Respecter les couches `app`, `domain`, `infra`, `ui`, `render3d` et `tests`.
- QML affiche et délègue ; le protocole cloud, MQTT, cache et sécurité reste en C++.
- La configuration broker Anycubic documentée est un contrat de compatibilité figé.
- `ignoreSslErrors()` reste limité au téléchargement de miniatures vers le cache local.
- Ne jamais journaliser tokens, cookies, sessions, clés privées ou URLs signées complètes.
- Ne jamais inclure build, caches, logs, HAR, `session.json` ou secrets dans un patch.
- Ne pas refondre une zone hors périmètre demandé.
- Le viewer rapide utilise un pas fixe d’une couche sur deux ; aucun mode `layer_step = 1` n’est exposé par l’UI.
- L’option Supports déclenche une analyse sémantique en deux passes natives (bas→haut depuis la fin du radeau, puis haut→bas depuis la dernière couche) avant le maillage stride-2 ; la géométrie matière reste issue du masque PWSZ d’origine.

## Commandes standard

Boucle de développement desktop depuis `accloud/` :

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure
```

Le gate de clôture Qt complet reste `local-full`. Pour toute modification du viewer, utiliser en plus `experimental-viewer-core` puis `experimental-viewer-qt` selon le périmètre.

## Politique de modification

Avant de modifier, identifier le runtime réellement chargé et le module propriétaire du comportement. Un échec de validation obligatoire arrête la chaîne de livraison ; il ne doit pas être masqué par une réparation opportuniste.

Lorsque la demande utilisateur porte sur un commit, il est interdit d'exécuter des tests avant ce commit sauf demande explicite de l'utilisateur. En dehors d'une demande de commit, les validations pertinentes peuvent être lancées normalement.

Ne jamais commit ni push sans instruction explicite.
