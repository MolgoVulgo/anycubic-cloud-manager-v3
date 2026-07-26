# AGENTS.md — Anycubic Cloud Manager V3

Ce fichier définit les contraintes projet chargées avant toute intervention.

## Cadre

Le dépôt est un projet C++20 / Qt6 / QML construit avec CMake. Python est réservé au tooling et aux contrôles annexes. Il ne doit pas devenir la référence architecturale de l'application.

## Ordre de lecture

1. `regles-generales-production-correctifs.md` pour produire et livrer un correctif ;
2. `codex-patch-mode.md` uniquement lorsqu'un patch ZIP déjà produit doit être appliqué mécaniquement ;
3. `accloud/CMakeLists.txt` et `accloud/CMakePresets.json` ;
4. `docs/README.md` ou `docs/FR/README.md` ;
5. le document fonctionnel correspondant à la zone réellement touchée ;
6. le code actif et les tests déclarés par CMake.

Les documents sous `docs/FR/sources-techniques/` sont des matériaux historiques ou d'investigation. Ils ne remplacent jamais la documentation principale ni le runtime actif.

## Points d'entrée actifs

- bootstrap : `src/accloud/app/main.cpp` ;
- ressources QML : `src/accloud/app/resources.qrc` ;
- ressources debug : `src/accloud/app/resources_debug.qrc` ;
- fenêtre principale : `src/accloud/ui/qml/MainWindow.qml` ;
- cloud : `src/accloud/infra/cloud/` ;
- MQTT : `src/accloud/infra/mqtt/` et `src/accloud/app/MqttBridge.cpp`.

## Invariants

- Respecter les couches `app`, `domain`, `infra`, `ui`, `render3d` et `tests`.
- QML affiche et délègue ; le protocole cloud, MQTT, cache et sécurité reste en C++.
- La configuration broker Anycubic documentée est un contrat de compatibilité figé.
- `ignoreSslErrors()` reste limité au téléchargement de miniatures vers le cache local.
- Ne jamais journaliser tokens, cookies, sessions, clés privées ou URLs signées complètes.
- Ne jamais inclure build, caches, logs, HAR, `session.json` ou secrets dans un patch.
- Ne pas refondre une zone hors périmètre demandé.

## Commandes standard

Depuis `accloud/` :

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure
```

Les validations doivent rester ciblées sauf demande explicite de full gate.

## Politique de modification

Avant de modifier, identifier le runtime réellement chargé et le module propriétaire du comportement. Un échec de validation obligatoire arrête la chaîne de livraison ; il ne doit pas être masqué par une réparation opportuniste.

Ne jamais commit ni push sans instruction explicite.
