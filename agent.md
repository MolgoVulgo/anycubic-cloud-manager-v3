# agent-neutral.md

## Contexte d’utilisation

Ce fichier définit le cadre de fonctionnement d’un **agent Codex (extension VS Code)** pour des projets **logiciels** (embarqué ou non).

* Types de projets : **embarqué / firmware**, **backend**, **CLI**, **applications desktop/web**
* Langages fréquents : **C/C++**, **Rust**, **Python**, **TypeScript/JavaScript**, **Go**
* Systèmes de build possibles : **CMake / Make / Ninja**, **PlatformIO**, **cargo**, **pip/poetry**, **npm/pnpm/yarn**

---

## Contraintes (générales)

* Ressources et budgets potentiellement contraints (CPU / RAM / stockage / latence).
* Chemins d’exécution critiques possibles (boucle principale, tâches/threads, handlers I/O).
* Toute modification doit respecter :
  * contraintes de performance,
  * stabilité runtime,
  * compatibilité (API, formats, config, plateformes).

---

## Objectifs prioritaires

1. **Fonctionnement correct dans l’environnement cible** (machine, conteneur, carte, navigateur, etc.)
2. **Stabilité** (pas de crash, pas de watchdog/reset, pas de fuites critiques)
3. **Lisibilité et maintenabilité**
4. **Modifications minimales et localisées**

---

## Style et posture de l’agent

* Direct, orienté exécution.
* Peu de texte, code en priorité.
* Pas de pédagogie, sauf si demandé.
* Questionner **uniquement** si une ambiguïté bloque l’implémentation.
* Langue : **français par défaut** (ou la langue explicitement demandée).

---

## Paramètres du projet (à déduire du dépôt)

À identifier **dans le dépôt** (sans supposer) :

* Système de build et commandes (ex : `CMakeLists.txt`, `Makefile`, `package.json`, `pyproject.toml`, `Cargo.toml`, `platformio.ini`).
* Point(s) d’entrée (ex : `main()`, service entrypoint, tâche RTOS, handler web).
* Cibles supportées (OS/arch, carte, versions runtime).
* Dépendances et versions.
* Parties générées automatiquement (UI, API clients, codegen) et règles de modification.

---

## Lecture du dépôt (ordre imposé)

1. Fichiers de build / config racine (ex : `CMakeLists.txt`, `package.json`, `pyproject.toml`, `Cargo.toml`, `platformio.ini`)
2. Arborescence : `src/`, `lib/`, `components/`, `include/`, `app/` (selon le langage)
3. Configs spécifiques (ex : `sdkconfig`, `*.env`, `config/*.yaml`, `partitions`, `docker-compose.yml`)
4. Documentation : `README*`, `docs/`, `DOCUMENTATION.md` si présent
5. Vérifier la cohérence des **interfaces** entre code applicatif et code généré (types, signatures, contrats, versions) **avant** toute analyse approfondie.

---

## Check-list de validation (rapide)

* Le système de build est identifié et reproductible.
* Les interfaces publiques (headers, types, schémas, DTO) sont cohérentes.
* Les fichiers générés / vendored sont respectés (pas de modification directe si interdit).
* Les logs peuvent être activés/désactivés (niveau debug) sans changer le code fonctionnel.
* Les erreurs sont gérées (retours, exceptions, codes d’erreur) sur les chemins critiques.

---

## Politique de modification du code

* **Appliquer immédiatement** la solution la plus probable.
* **Pas de discussion** tant que le code compile/build et fonctionne dans la cible définie.
* Respecter strictement les conventions du repo (formatage, lint, architecture).
* Ne pas mélanger des paradigmes incompatibles (ex : async vs threads sans stratégie, frameworks concurrents).
* **Code généré** :
  * Si un dossier est généré (ex : `generated/`, `ui/`, `dist/`, `build/`), ne pas le modifier sauf règle explicite du projet.
  * Si un problème vient du généré, **l’expliquer clairement** et corriger via la source de génération ou via un wrapper.

---

## Tests

* Utiliser **ce que le repo prévoit** (unitaires, intégration, e2e).
* Si aucun test n’existe :
  * proposer un scénario de validation minimal (commandes, étapes, jeux de données),
  * privilégier une vérification “réelle” (environnement cible) quand applicable.

---

## Debug et logs (obligatoire)

* Implémenter un système de logs cohérent (tags/modules, niveaux, timestamps si utile).

### Principes

* Niveaux : `DEBUG / INFO / WARN / ERROR` (au minimum).
* Activation/désactivation du debug via :
  * flag de build (ex : `-DDEBUG_LOG`, feature flag `cfg(debug_logs)`),
  * ou variable d’environnement/config (selon la stack).

### Exemples (indicatifs)

* C/C++ : macros `LOGI/LOGW/LOGE`, ou lib de logging du projet.
* Rust : `log` + backend (`env_logger`, `tracing`).
* Python : module `logging`.
* Node : logger structuré (`pino`, `winston`) si déjà présent.

---

## Qualité et conventions

* Éviter allocations/IO inutiles sur chemins chauds.
* Éviter blocages longs dans threads/boucles critiques.
* Gestion explicite des erreurs et des timeouts.
* Code lisible, structuré, prévisible.

---

## Formats de sortie attendus

* Fichiers prêts à build/runner dans le contexte du repo.
* Commandes de build/run claires (ou patch sur la doc si manquantes).
* Flags/config requis explicités.
* Si une information critique manque (cible, build tool, point d’entrée), **demander explicitement**.

---

## Structure de réponse attendue

* Constat technique
* Action(s) appliquée(s)
* Étapes de validation (dans l’environnement cible)
* Hypothèses (si nécessaires)

Après toute génération ou modification de code : analyser le diff réel et les fichiers impactés afin de produire un **message de commit en anglais**, à l’impératif, reflétant l’intention principale du changement et sa portée (fonctionnelle/technique), conforme aux conventions Git (pas de résumé vague).
---

## Commits Git (discipline et génération)

* Faire un commit **dès qu’un changement forme une unité logique, testable et cohérente**.
* Éviter les commits trop gros **ou** trop fragmentés : chaque commit doit raconter **une intention claire et isolée**.
* Si plusieurs intentions distinctes apparaissent, **split** en commits séparés (cohérents, minimaux).
* **Avant chaque commit**, mettre à jour la documentation impactée (`README*`, `Docs/`, etc.).
* Si une modification n’est couverte par aucun document existant, **créer un nouveau document** et l’ajouter à la documentation du dépôt.

### Règles de message

* Les **messages de commit doivent être en anglais**.
* Le message (et le découpage éventuel en plusieurs commits) doit être construit **à partir des modifications depuis le dernier commit** :
  * baser le résumé sur `git status`, `git diff --stat HEAD`, et `git diff HEAD`
  * ne pas inventer : le message doit décrire **exactement** ce que le diff montre
* Forme : impératif, intention principale (ex : `Fix pw0Img RLE decoding`, `Add GPU draw pipeline contracts`).

### Prompt standard : Commit Generator

Utiliser ce prompt pour générer un message de commit orienté audit/PR review :

```text
Rédige un message de commit Git en anglais, orienté audit/PR review.

Contraintes:
- Titre en anglais, format conventional commit: <type>(<scope>): <summary>
- Corps en puces, concret, sans phrases vagues.
- Mentionner explicitement:
  - changements fonctionnels,
  - robustesse/gestion d’erreurs,
  - tests ajoutés ou modifiés,
  - docs mises à jour.
- Ajouter une ligne finale "Tests:" avec la commande exécutée et le résultat.
- Ajouter une ligne finale "Docs:" avec les fichiers de documentation touchés.
- Ne pas inventer d’éléments non présents dans le diff.

Format attendu:
<type>(<scope>): <summary>

- ...
- ...
- ...

Tests: <commande> (<résultat>)
Docs: <liste de fichiers ou "none">
```

Exemple attendu :

```text
feat(render3d): harden viewer reliability and complete remaining QA tasks

- Add robust OpenGL failure handling in PWMB 3D dialog (init/draw/upload fallback paths).
- Add user-facing build error classification (parse/decode/GL) and "Retry last build" flow.
- Persist viewer settings across sessions (threshold/bin mode/quality/stride/contour-only).
- Enforce strict index mask semantics (color_index != 0) via dedicated PW0 nonzero-mask decoding.
- Add camera-based back-to-front layer ordering for translucent rendering.
- Add integration tests for async viewer build, retry behavior, and renderer-failure UX.
- Add minimal GUI E2E flow: Files -> Viewer -> rebuild -> cutoff/stride.
- Add golden non-regression test (orientation/bbox/checksum) for PWMB geometry outputs.
- Update remaining-task tracker and lot documentation.

Tests: PYTHONPATH=. pytest -q (112 passed)
Docs: RESTE_A_FAIRE.md, docs/52_LOT_J_VIEWER_RELIABILITY_AND_QA.md
```
