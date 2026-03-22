# Anycubic Cloud Manager V3

Statut documentaire : `IMPLEMENTE`

## Rôle

Dépôt **C++20 / Qt6 / QML** centré sur :

* le **client cloud Anycubic** ;
* le **runtime MQTT** ;
* la **documentation technique consolidée** ;
* la **trajectoire viewer / formats Photon**.

Le dépôt sert à la fois de base de développement, de support documentaire consolidé et de point de convergence entre le code existant, les flux cloud et les travaux viewer à venir.

## Périmètre actuel

Le projet couvre principalement :

* l’intégration des flux cloud Anycubic ;
* la gestion du runtime MQTT associé ;
* l’UI Qt / QML du client ;
* la documentation consolidée du fonctionnement courant ;
* la préparation de la phase viewer / formats Photon.

La partie **viewer** reste un chantier distinct et n’est pas considérée ici comme totalement implémentée.

## Structure documentaire

La documentation active est organisée autour de deux niveaux :

### 1. Socle consolidé transverse

Ordre de lecture recommandé :

1. `Docs/00_documentation_consolidee_index.md`
2. `Docs/01_core_web_cloud_sync.md`
3. `Docs/02_ui_qml.md`
4. `Docs/03_photon_viewer_formats.md`

### 2. Sections spécialisées

* `Docs/MQTT/README.md`
* `Docs/i18n/README.md`
* `Docs/Anycubic Cloud Client/README.md`
* `Docs/Photon Viewer/README.md`
* `Docs/photon_files/README.md`

## Convention de numérotation

* le **socle consolidé** utilise une numérotation transverse `00..03` ;
* les **sections spécialisées** utilisent leur propre convention locale, décrite dans chaque `README.md` de section.

## Règle de vérité

L’ordre de priorité à retenir est le suivant :

1. **le code du dépôt** pour trancher le comportement réel ;
2. **le socle documentaire consolidé** pour la lecture transverse ;
3. **les sections spécialisées** pour les domaines ciblés ;
4. **les annexes / snapshots** uniquement comme support de contexte.

Une documentation ne doit pas être lue comme normative si elle contredit le comportement réel du dépôt.

## Build et exécution

Le projet repose sur **CMake** avec des presets de build. La lecture normale du dépôt part des fichiers racine habituels :

* `CMakeLists.txt`
* `CMakePresets.json`
* `accloud/`
* `Docs/`

Les détails d’implémentation, d’architecture et de périmètre fonctionnel doivent être lus dans les documents consolidés plutôt que reconstruits à partir de fragments historiques.

## Point d’entrée documentaire

Pour une lecture structurée du dépôt, commencer par :

* `Docs/README.md`

Puis suivre le socle `00..03` avant d’ouvrir les sections spécialisées.

## Statuts documentaires

Le corpus utilise les statuts suivants :

* `IMPLEMENTE`
* `PARTIEL`
* `SPEC`
* `SNAPSHOT`

Chaque document doit annoncer explicitement son statut et être lu selon ce cadrage.

## Résumé rapide

Ce dépôt est la base active du projet **Anycubic Cloud Manager V3** pour :

* le client cloud ;
* MQTT ;
* l’UI Qt / QML ;
* la documentation consolidée ;
* la préparation du viewer Photon.

Le bon point d’entrée n’est pas un document isolé, mais le couple :

* `README.md` racine
* `Docs/README.md`
