# agent-accloud.md

## Contexte du dépôt

Ce fichier définit le cadre de travail d’un **agent Codex** pour `anycubic-cloud-manager-v3`.

Le dépôt doit être traité en priorité comme un projet :
- **C++20 / Qt6 / QML**
- architecture en couches
- build / test via **CMake presets**

---

## Règle de cadrage

- Traiter ce dépôt comme un projet **C++ / Qt** en priorité
- Ne pas appliquer des habitudes Python comme référence principale
- Python peut exister pour du tooling ou des tests annexes, mais :
  - pas de refonte de l’architecture autour de Python
  - pas de `pytest` par défaut si le dépôt ne le prévoit pas
  - pas de conventions Python imposées au code applicatif Qt

---

## Sources de vérité du projet

Lire d’abord :
1. `accloud/CMakeLists.txt`
2. `accloud/CMakePresets.json`
3. `accloud/README.md`
4. `Docs/00_documentation_consolidee_index.md`
5. la zone fonctionnelle réellement touchée

Points d’entrée utiles :
- `accloud/app/main.cpp`
- `accloud/app/MqttBridge.cpp`
- `accloud/app/MqttBridge.h`

---

## Commandes standard

Depuis `accloud/` :

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure
```

Build modes :
- `default` : Debug + Qt ON + `ACCLOUD_DEBUG=OFF`
- `dev-debug` : Debug + Qt ON + `ACCLOUD_DEBUG=ON`
- `prod` : Release + Qt ON + `ACCLOUD_DEBUG=OFF`

Commandes ciblées fréquentes :

```bash
cmake --build --preset default --target accloud_cli
ctest --preset default -R accloud_mqtt_flow --output-on-failure
```

---

## Contraintes techniques du dépôt

- Garder la séparation des couches :
  - `app`
  - `domain`
  - `infra`
  - `ui`
  - `render3d`
  - `tests`
- Ne pas injecter de logique métier dans QML si un bridge ou un store C++ existe déjà
- Respecter le mode MQTT runtime documenté
- Ne jamais logger les secrets
- Conserver des changements localisés et testables

---

## Politique de modification

- Modifier uniquement les fichiers nécessaires à l’intention du changement
- Ne pas toucher aux artefacts générés ou de build (`accloud/build/`, binaires, caches), sauf demande explicite
- Si un changement impacte le debug MQTT ou le comportement utilisateur, mettre à jour la doc associée dans `Docs/`
- En cas de doute architectural, préférer une correction minimale dans le module déjà responsable

### Collaboration obligatoire

- Ne jamais générer du code en solo
- Toujours détailler ce qui va être fait avant toute modification de code

---

## Validation attendue

Avant de conclure :
- build de la cible concernée
- tests ciblés ou preset complet si impact large
- vérification rapide des régressions évidentes sur l’UI/QML si la zone est touchée

Si un test ne peut pas être exécuté :
- le signaler explicitement
- donner la raison réelle
  - dépendance manquante
  - broker live indisponible
  - session non disponible
  - environnement incomplet

---

## Règles spécifiques MQTT / secrets

- Respecter la documentation de `Docs/MQTT/`
- Respecter auth slicer + mTLS si le dépôt l’impose
- Redacter strictement :
  - tokens
  - credentials
  - données de session
  - valeurs sensibles dans logs et traces

---

## Structure de réponse attendue

- Constat technique
- Actions appliquées
- Validation exécutée
- Limites / hypothèses, si nécessaires

Style attendu :
- direct
- court
- orienté exécution

---

## Commits Git

- Faire des commits par unité logique cohérente et testable
- Ne jamais faire de commit automatiquement sans demande explicite de l’utilisateur
- Messages en anglais
- ne jamais les pousser soi meme toujours demander
- Conventional commit recommandé :

```text
<type>(<scope>): <summary>
```

- Le message doit décrire exactement le diff réel :
  - `git status`
  - `git diff --stat HEAD`
  - `git diff HEAD`
- Mentionner dans le corps :
  - changements fonctionnels
  - robustesse / gestion d’erreurs
  - tests exécutés
  - docs mises à jour

### Template

```text
<type>(<scope>): <summary>

- ...
- ...

Tests: <commande> (<résultat>)
Docs: <liste de fichiers ou "none">
```
