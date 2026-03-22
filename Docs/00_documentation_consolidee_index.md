# Documentation consolidee — Anycubic Cloud Manager V3

## Objet

Ce corpus fournit un noyau documentaire unique, organise par themes et aligne sur l'etat reel du code.

Principes:
- un document principal par theme ;
- priorite au code reel en cas d'ecart ;
- extension par sections specialisees sans duplication du socle.

---

## Structure retenue

### 1. Core Web / Cloud Sync
- `01_core_web_cloud_sync.md`
- cible : architecture web, endpoints, cache local, session, strategie de sync, trajectoire de correction.

### 2. UI / QML
- `02_ui_qml.md`
- cible : shell UI, pages, dialogs, design system, onglets, lots UI, dette visible et cadrage d'implementation.

### 3. Photon / Viewer / Formats
- `03_photon_viewer_formats.md`
- cible : structure viewer, etat reel vs cible, formats photons, perimetre reellement present et non commence.

### 4. MQTT
- `MQTT/README.md`
- cible : architecture runtime MQTT, routage, topics, telemetrie et structures de messages.

### 5. i18n
- `i18n/README.md`
- cible : etat reel de l'internationalisation, pipeline Qt/QML, regles de traduction et chantiers restants.

### 6. Decoupage UI detaille
- `Anycubic Cloud Client/README.md`
- `Photon Viewer/README.md`

---

## Regles de lecture

### Statuts documentaires
- `IMPLEMENTE` : decrit un comportement visible dans le code actuel.
- `PARTIEL` : commence mais non ferme.
- `SPEC` : cible recommandee, pas encore entierement portee par le code.
- `SNAPSHOT` : photographie utile mais non normative.

### Regle d'arbitrage
En cas de conflit entre:
1. un snapshot,
2. une spec,
3. le code reel,

la hierarchie est:
- **code reel** d'abord,
- **document principal du theme** ensuite,
- **snapshot** en dernier.

---

## Ordre de lecture recommande

1. `01_core_web_cloud_sync.md`
2. `02_ui_qml.md`
3. `03_photon_viewer_formats.md`
4. `MQTT/README.md`
5. `i18n/README.md`
6. `Anycubic Cloud Client/README.md`
7. `Photon Viewer/README.md`

---

## Convention d'entretien

Toute nouvelle documentation doit:
- se rattacher a un theme unique ;
- declarer son statut ;
- preciser si elle decrit l'existant ou la cible ;
- eviter les doublons avec ce corpus ;
- etre referencee depuis le README de sa section.
