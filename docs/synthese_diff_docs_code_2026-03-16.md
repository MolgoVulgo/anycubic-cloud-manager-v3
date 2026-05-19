# Synthese des ecarts Documentation vs Code

Date d'analyse: 2026-03-16.
Perimetre analyse: `Docs/` (documentation) et `accloud/` (code source).

## Methode

- Lecture de la cartographie doc (`Docs/README.md`) et des pages de statut (`IMPLEMENTE`, `PARTIEL`, `SPEC`, `SNAPSHOT`).
- Verification des points critiques cote code: UI QML, endpoints cloud, etat des modules scaffold, pipeline de tests.
- Verification par commandes: inventaire endpoints, recherche de placeholders, verification UI migration, et tentative d'execution des tests CTest.

## Synthese executive

1. **Alignement global bon sur la separation des statuts**: la doc annonce explicitement les zones `SPEC/PARTIEL` et le code confirme qu'une partie de ces zones reste en scaffold (photons/render/jobs/cache infra).
2. **Ecarts principaux identifies**:
   - **Chemin demande utilisateur `/docs`** vs realite repo `Docs/` (majuscule).
   - **Section tests dans `etat_reel_vs_cible.md` obsolete**: la doc cite un resultat `3/3 passes`, alors que l'execution actuelle dans l'environnement ne trouve aucun test sans build configure.
   - **Contrat endpoints v2 non totalement harmonise** entre pages doc historiques et endpoint registry C++ (ex: printer info en `POST /work/printer/Info` dans une page, mais `GET /v2/printer/info` dans le code).
3. **UI Cloud Client**: la doc de migration/tabs est coherente avec le code courant (check migration OK, tabs Files/Printers/MQTT/Logs presentes).


## Niveau global des ecarts

- **Evaluation globale**: ecarts **plutot mineurs a moderes**.
- Les ecarts identifies sont majoritairement de **coherence documentaire** (naming de chemin, clarification des sources de verite endpoints, section tests a rendre reproductible).
- A ce stade, l'analyse ne met pas en evidence de divergence fonctionnelle critique entre comportement runtime principal et documentation `IMPLEMENTE`.

## Ecarts detailles

## 1) Structure et acces documentation

- **Constat**: le depot utilise `Docs/` (D majuscule), pas `docs/`.
- **Impact**: risque de scripts/consignes cassant sur systemes sensibles a la casse.
- **Action recommandee**: standardiser la convention dans les consignes externes (ou ajouter un alias/symlink si necessaire).

## 2) Etat des modules "non livrés" (coherent avec la doc)

- **Doc**: la cartographie etat reel/cible marque explicitement des zones non finalisees (photons, render3d, jobs, cache infra).
- **Code**: presence de nombreux fichiers `Scaffold placeholder` dans ces memes zones.
- **Conclusion**: pas un conflit, mais une confirmation que ces zones restent a livrer.

## 3) Endpoints cloud: derive documentaire partielle

- **Doc**: `end_points_v2_verifie.md` distingue deja endpoints v2 prouves vs catalogue elargi.
- **Code**: l'implementation runtime s'appuie sur `EndpointRegistry` avec un set concret d'endpoints (mix `work/*` et `v2/*`).
- **Ecart notable**:
  - la doc recense `POST /p/p/workbench/api/work/printer/Info` (heritage v2 Python),
  - le code consomme `GET /p/p/workbench/api/v2/printer/info`.
- **Conclusion**: la doc devrait clarifier explicitement la **source de verite runtime C++** vs references historiques Python.

## 4) Tests et preuves d'execution

- **Doc**: `etat_reel_vs_cible.md` mentionne une execution de reference `ctest --preset default` avec `3/3` passes.
- **Code/build actuel**:
  - pas de `CMakePresets.json` a la racine du repo,
  - dans `accloud/`, `ctest --preset default` s'execute mais ne trouve aucun test si aucun build/test tree n'est configure.
- **Conclusion**: preuve de test doc non reproductible telle quelle dans l'etat present; la section doit etre mise a jour pour indiquer preconditions exactes (generation build dir + ctest depuis ce build dir).

## 5) UI Cloud Client

- **Verification**: check migration UI passe (`python3 accloud/tools/check_ui_migration.py`).
- **Code**: onglets principaux disponibles (`Files`, `Printers`, `MQTT`, `Logs`) dans `MainWindow.qml`.
- **Conclusion**: pas d'ecart majeur detecte sur la structure UI de base annoncee comme implementee.

## Recommandations prioritaires

1. Mettre a jour `Docs/etat_reel_vs_cible.md` (date + section tests reproductible + nombre de tests reel).
2. Ajouter une section "Source de verite endpoints runtime" dans `Docs/end_points_v2_verifie.md` pointant explicitement vers `EndpointRegistry.cpp`.
3. Ajouter en tete de `Docs/README.md` une note "dossier officiel: `Docs/`" pour eviter la confusion `/docs`.
4. Conserver la distinction `SPEC/PARTIEL` qui est saine et globalement fidele au code actuel.
