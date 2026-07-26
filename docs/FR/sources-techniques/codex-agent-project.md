# Codex agent — règles détaillées projet anycubic-cloud-manager-v3

Ce document complète `agent.md`.
Il contient les règles projet longues. `agent.md` reste volontairement compact pour limiter le contexte chargé en permanence.

## 1. Contexte du dépôt

Ce dépôt doit être traité comme un projet applicatif :

- C++20 ;
- Qt6 ;
- QML ;
- CMake presets ;
- architecture en couches.

La présence éventuelle de Python ne change pas le cadrage principal. Python peut servir au tooling, aux scripts ou à des tests annexes, mais ne doit pas devenir la référence architecturale du dépôt.

## 2. Cadrage non négociable

- Projet principal : C++ / Qt / QML.
- Build et tests : CMake presets.
- Ne pas appliquer de conventions Python au code applicatif Qt.
- Ne pas lancer `pytest` par défaut si le dépôt ne le prévoit pas.
- Ne pas refondre l’architecture autour de Python.
- Ne pas déplacer la logique métier dans QML si un bridge, un store ou un service C++ existe déjà.
- Ne pas introduire de dépendance ou d’architecture nouvelle sans nécessité claire.

## 3. Sources de vérité

Lire d’abord, dans cet ordre :

1. `accloud/CMakeLists.txt`
2. `accloud/CMakePresets.json`
3. `accloud/README.md`
4. `Docs/00_documentation_consolidee_index.md`
5. la documentation spécifique à la zone touchée
6. la zone fonctionnelle réellement modifiée
7. les fichiers de configuration ou d’entrée du module modifié

Points d’entrée utiles :

- `accloud/app/main.cpp`
- `accloud/app/MqttBridge.cpp`
- `accloud/app/MqttBridge.h`

## 4. Architecture à préserver

Respecter la séparation des couches :

- `app`
- `domain`
- `infra`
- `ui`
- `render3d`
- `tests`

Règles de séparation :

- `ui` / QML ne doit pas porter la logique métier si une abstraction C++ existe.
- `app` orchestre, mais ne doit pas absorber toute la logique.
- `domain` porte les règles métier stables.
- `infra` porte les accès externes, protocoles, fichiers, réseau, MQTT et détails techniques.
- `render3d` reste isolé des flux métier non nécessaires.
- `tests` doit suivre les responsabilités réellement modifiées.

## 5. Commandes standard

Depuis `accloud/` :

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure
```

Build modes connus :

- `default` : Debug + Qt ON + `ACCLOUD_DEBUG=OFF`
- `dev-debug` : Debug + Qt ON + `ACCLOUD_DEBUG=ON`
- `prod` : Release + Qt ON + `ACCLOUD_DEBUG=OFF`

Commandes ciblées fréquentes :

```bash
cmake --build --preset default --target accloud_cli
ctest --preset default -R accloud_mqtt_flow --output-on-failure
```

Règle : ne pas inventer de commande. Si un preset, une cible ou un test n’existe pas, le signaler explicitement.

## 6. Règles MQTT / auth / secrets

- Respecter la documentation de `Docs/MQTT/`.
- Respecter le mode MQTT runtime documenté.
- Respecter auth slicer et mTLS si le dépôt les impose.
- Ne jamais logger :
  - tokens ;
  - credentials ;
  - données de session ;
  - clés privées ;
  - cookies ;
  - valeurs sensibles issues des échanges réseau.
- Redacter les valeurs sensibles dans les logs, traces, exemples et sorties de debug.
- Ne pas rendre persistante une donnée sensible sans demande explicite et justification technique.

## 7. Politique de modification projet

- Modifier uniquement les fichiers nécessaires à l’intention du changement.
- Conserver les changements localisés, lisibles et testables.
- Ne pas toucher aux artefacts générés ou de build : `accloud/build/`, binaires, caches, outputs CMake.
- Ne pas modifier les dépendances, lockfiles ou fichiers générés sans nécessité explicite.
- Si un changement impacte le debug MQTT ou le comportement utilisateur, mettre à jour la documentation associée dans `Docs/`.
- En cas de doute architectural, préférer une correction minimale dans le module déjà responsable.
- Ne pas introduire de fonctionnalité hors périmètre sans demande explicite.

## 8. Collaboration obligatoire

- Ne jamais générer du code en solo.
- Toujours exposer le constat technique avant modification.
- Toujours détailler ce qui va être modifié avant d’écrire du code.
- Toujours expliciter les hypothèses réelles quand une structure, un flux MQTT, une session ou un composant UI n’est pas confirmé.

## 9. Validation attendue

Avant de conclure :

- build de la cible concernée ;
- tests ciblés liés au changement ;
- preset complet si l’impact est large ;
- vérification rapide des régressions évidentes sur l’UI/QML si la zone est touchée ;
- vérification de la documentation si un comportement utilisateur, debug ou MQTT change.

Si une validation ne peut pas être exécutée, indiquer la raison réelle :

- dépendance manquante ;
- Qt/CMake absent ;
- broker live indisponible ;
- session non disponible ;
- environnement incomplet ;
- commande non définie ;
- dépôt incomplet.

Ne pas prétendre avoir compilé, testé, connecté un broker ou validé une UI si ce n’est pas le cas.

## 10. Réponse attendue dans ce projet

Structure par défaut après intervention :

- Constat technique
- Actions appliquées
- Validation exécutée
- Limites / hypothèses, si nécessaires

Style : direct, court, orienté exécution, sans code si l’utilisateur ne le demande pas.

## 11. Git — règles projet

Les règles Git générales sont dans `~/.codex/config.toml`.

Règles spécifiques projet :

- Commit uniquement sur demande explicite de l’utilisateur.
- Push uniquement sur demande explicite de l’utilisateur.
- Un commit = une unité logique cohérente et testable.
- Mentionner l’impact documentation si `Docs/` est modifié ou aurait dû l’être.
- Mentionner l’impact MQTT, auth, mTLS ou comportement utilisateur si concerné.

Scopes recommandés :

- `app`
- `domain`
- `infra`
- `ui`
- `render3d`
- `mqtt`
- `docs`
- `tests`
- `build`
- `ci`

Le message doit rester basé sur le diff réel, sans promesse non vérifiée.
