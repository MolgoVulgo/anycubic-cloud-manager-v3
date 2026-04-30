# agent.md — anycubic-cloud-manager-v3

Ce fichier définit les contraintes projet que Codex doit charger en premier.
Le détail long est dans `Docs/codex-agent-project.md`.

Le dépôt doit être traité en priorité comme un projet :
- C++20 / Qt6 / QML ;
- architecture en couches ;
- build et tests via CMake presets.

## Cadrage projet

- Traiter ce dépôt comme un projet C++ / Qt en priorité.
- Ne pas utiliser Python comme référence architecturale principale.
- Python peut exister pour du tooling ou des tests annexes, mais ne doit pas piloter l’architecture applicative.
- Ne pas plaquer `pytest`, conventions Python ou structure Python sur le code applicatif Qt.
- Ne pas refondre l’architecture hors périmètre validé.

## Sources de vérité

Lire d’abord :
1. `accloud/CMakeLists.txt`
2. `accloud/CMakePresets.json`
3. `accloud/README.md`
4. `Docs/00_documentation_consolidee_index.md`
5. `Docs/codex-agent-project.md`
6. la zone fonctionnelle réellement touchée

Points d’entrée utiles :
- `accloud/app/main.cpp`
- `accloud/app/MqttBridge.cpp`
- `accloud/app/MqttBridge.h`

## Commandes standard

Depuis `accloud/` :

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure
```

Commandes ciblées fréquentes :

```bash
cmake --build --preset default --target accloud_cli
ctest --preset default -R accloud_mqtt_flow --output-on-failure
```

Ne pas inventer de commande si le dépôt ne l’expose pas.

## Invariants techniques

- Respecter la séparation des couches : `app`, `domain`, `infra`, `ui`, `render3d`, `tests`.
- Ne pas injecter de logique métier dans QML si un bridge, un store ou un service C++ existe déjà.
- Respecter le mode MQTT runtime documenté.
- Respecter auth slicer et mTLS si le dépôt les impose.
- Ne jamais logger tokens, credentials, sessions, clés privées ou valeurs sensibles.
- Ne pas toucher aux artefacts générés ou de build : `accloud/build/`, binaires, caches, outputs CMake.
- Garder les changements minimaux, localisés et testables.

## Politique de modification

- Modifier uniquement les fichiers nécessaires à l’intention du changement.
- Si un changement impacte le debug MQTT ou le comportement utilisateur, mettre à jour la documentation associée dans `Docs/`.
- En cas de doute architectural, corriger dans le module déjà responsable plutôt que déplacer la responsabilité.
- Ne jamais générer du code en solo : exposer d’abord le constat technique et ce qui va être modifié.

## Validation attendue

Avant de conclure :
- build de la cible concernée ;
- tests ciblés ou preset complet si impact large ;
- vérification rapide des régressions évidentes UI/QML si la zone est touchée.

Si une validation ne peut pas être exécutée, donner la raison réelle : dépendance manquante, broker live indisponible, session absente, environnement incomplet ou commande non définie.

## Git

Appliquer les règles globales du `~/.codex/config.toml`.

Scopes recommandés pour ce dépôt :
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

Ne jamais commit ni push sans demande explicite.
