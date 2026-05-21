# Architecture projet

Statut : `IMPLEMENTE` pour la structure actuelle, `PARTIEL` pour la séparation long terme.

## Rôle

Anycubic Cloud Manager V3 est une application Linux desktop construite en C++20, Qt6 et QML. Le projet combine une UI desktop, un client Anycubic Cloud, l’observation MQTT temps réel, le cache/logging local et la base technique du viewer Photon/PWMB.

Le dépôt n’est pas un projet de scripts. Python peut servir au tooling, à l’analyse ou à la CI, mais la référence architecturale reste C++ / Qt / QML avec presets CMake.

## Frontières modules

```text
src/accloud/app/       bootstrap Qt, bridges QML, modèles exposés UI
src/accloud/domain/    types stables, vocabulaire métier, contrats
src/accloud/infra/     HTTP/cloud, MQTT, cache, logs, formats fichiers, jobs
src/accloud/render3d/  rendu OpenGL / bridge Qt Quick
src/accloud/ui/qml/    shell QML, pages, dialogs, composants visuels
```

Règle directe : QML affiche et délègue. QML ne doit pas devenir l’endroit où vivent la sync cloud, le routing MQTT, la politique cache ou le workflow imprimante.

## Points solides actuels

- Structure C++ réelle basée sur CMake.
- Frontend desktop Qt/QML déjà en place.
- Comportement cloud séparé en APIs infra, builders, session et use cases.
- MQTT isolé avec routing, credentials/TLS et intégration realtime.
- Logs, cache et jobs représentés par des composants infra explicites.
- Debug tooling excluable des builds production via `ACCLOUD_DEBUG`.

## Zones encore trop concentrées

L’ancienne analyse identifiait correctement trois risques :

1. le code cloud legacy peut encore masquer trop de comportement endpoint ;
2. les bridges peuvent devenir des hubs d’orchestration si UI, domaine et infra s’y mélangent ;
3. la sync existe, mais son contrat doit rester explicite : scope, fraîcheur, fallback, reconstruction.

Structure cible :

- les types domaine définissent fichiers, imprimantes, sessions, jobs et événements MQTT ;
- l’infra parle protocoles et stockage ;
- les use cases coordonnent les actions ;
- les bridges exposent à QML des points d’entrée asynchrones ;
- QML conserve uniquement l’état visuel et la délégation.

## Modes de build

| Mode | Intention |
| --- | --- |
| `default` | Debug, Qt actif, debug tooling désactivé. |
| `dev-debug` | Debug avec UI debug, payloads debug et traces. |
| `prod` | Release, debug tooling exclu. |

`start.sh` est un wrapper pratique. La référence reste les presets CMake.

## Décision

Le projet garde un axe actif prioritaire : cloud manager d’abord, runtime MQTT ensuite, viewer Photon comme trajectoire préparée mais non fermée. La documentation ne doit pas présenter une zone `SPEC` comme livrée.
