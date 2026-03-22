# Documentation unifiée — Anycubic Cloud Manager V3

## Objet

Ce corpus remplace la dispersion initiale du paquet documentaire fourni dans `Archive.zip` par un noyau de lecture unique, découpé par thèmes.

Le principe est simple :
- un document = un sujet lisible ;
- les doublons sont absorbés ;
- les snapshots restent informatifs mais ne pilotent pas l’architecture ;
- les documents MQTT et i18n créés ici sont **complémentaires** aux documents déjà existants dans le projet et doivent être fusionnés avec eux, pas les écraser.

---

## Structure retenue

### 1. Core Web / Cloud Sync
- `01_core_web_cloud_sync.md`
- cible : architecture web, endpoints, cache local, session, stratégie de sync, trajectoire de correction.

### 2. UI / QML
- `02_ui_qml.md`
- cible : shell UI, pages, dialogs, design system, onglets, lots UI, dette visible et cadrage d’implémentation.

### 3. Photon / Viewer / Formats
- `03_photon_viewer_formats.md`
- cible : structure viewer, état réel vs cible, formats photons, périmètre réellement présent et non commencé.

### 4. MQTT — document complémentaire
- `Docs_Complement_MQTT.md`
- cible : architecture runtime MQTT réellement présente, routage, topics, télémétrie, limites et points de fusion avec la doc MQTT existante.

### 5. i18n — document complémentaire
- `Docs_Complement_i18n.md`
- cible : état réel de l’internationalisation, pipeline Qt/QML, règles de traduction et chantiers restants.

### 6. Plan d’action global
- `Docs_Plan_Action_modifications.md`
- cible : séquencement des travaux recommandés, ordre d’exécution, dépendances, critères de sortie.

---

## Règles de lecture

### Statuts documentaires
- `IMPLEMENTE` : décrit un comportement visible dans le code actuel.
- `PARTIEL` : commencé mais non fermé.
- `SPEC` : cible recommandée, pas encore entièrement portée par le code.
- `SNAPSHOT` : photographie utile mais non normative.

### Règle d’arbitrage
En cas de conflit entre :
1. un snapshot,
2. une spec,
3. le code réel,

la hiérarchie est :
- **code réel** d’abord,
- **spec consolidée** ensuite,
- **snapshot** en dernier.

---

## Cartographie des documents absorbés

### Core / Cloud / Runtime
Documents absorbés :
- `core_web.md`
- `core_web_implementation_lots.md`
- `end_points.md`
- `end_points_v2_verifie.md`
- `endpoints_reference_policy.md`
- `endpoints_capture_report.md`
- `etat_reel_vs_cible.md`
- `har.md`
- `local_cache_sync_strategy.md`
- `logging_reference.md`
- `debug_build_modes.md`
- `plan_de_correction.md`

### UI / QML
Documents absorbés :
- `ui_views.md`
- `ui_spec_cloud_remote_print_visual.md`
- `ui_lots_1_5_implementation.md`
- `synthese_qml_ui.md`
- `spec_dimplementation_qml.md`
- `correctif_onglets.md`
- `ui_theme_action_tasks.md`
- `printer_live_remote_print_flow.md`
- `print_tab_official_logs_snapshot.md`

### Photon / Viewer / Formats
Documents absorbés :
- `structure_application_photons.md`
- `photon_formats.md`
- les éléments viewer partiels présents dans les docs UI et d’état réel

---

## Décisions de consolidation

### Ce qui est considéré comme acquis
- l’application est d’abord un client cloud Qt/QML ;
- le viewer photons existe dans la cible, pas encore dans le produit ;
- la couche cloud réelle est déjà fonctionnelle mais reste trop concentrée ;
- la sync existe, mais sa logique de résilience n’est pas encore fermée ;
- l’UI Cloud est la partie la plus avancée du produit ;
- MQTT et i18n existent déjà en runtime et doivent maintenant être documentés au niveau exploitation/architecture.

### Ce qui n’est plus traité comme prioritaire
- toute lecture laissant penser que le viewer 3D est déjà engagé de manière substantielle ;
- toute lecture qui traite un snapshot de logs comme contrat produit ;
- toute architecture qui laisse `CloudBridge` absorber durablement du métier, du cache, de l’HTTP et de la sync.

---

## Ordre de lecture recommandé

1. `01_core_web_cloud_sync.md`
2. `02_ui_qml.md`
3. `Docs_Complement_MQTT.md`
4. `Docs_Complement_i18n.md`
5. `03_photon_viewer_formats.md`
6. `Docs_Plan_Action_modifications.md`

---

## Convention d’entretien

Toute nouvelle documentation doit :
- se rattacher à un thème unique ;
- déclarer son statut ;
- préciser si elle décrit l’existant ou la cible ;
- éviter les doublons avec ce corpus ;
- si elle touche MQTT ou i18n, être écrite comme extension du document complémentaire déjà créé.

